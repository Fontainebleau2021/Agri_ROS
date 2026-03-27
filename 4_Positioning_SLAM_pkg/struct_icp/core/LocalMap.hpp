#pragma once
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <Eigen/Core>
#include <sophus/se3.hpp>

#include "VoxelHashMap.hpp"

namespace struct_icp {

struct LocalMapParams {
  int window_size = 30;          // 保留最近K帧
  bool use_radius_crop = false;  // 是否按半径裁剪（可和window一起用）
  double crop_radius = 25.0;     // meters
  int rebuild_every_n = 1;       // 每N帧重建一次（1=每帧，2=隔帧）
  bool enable_async_rebuild = false;
  double rebuild_min_move = 0.0;
  int async_warmup_frames = 1;
  bool enable_assoc_map = false;
  double assoc_voxel = 1.0;
  bool enable_coarse_assoc = false;
  double coarse_voxel_mul = 2.0;
};

class LocalMap {
public:
  LocalMap(const VoxelMapParams& mp = {}, const LocalMapParams& lp = {})
      : mp_(mp), lp_(lp) {
    active_map_ = std::make_shared<VoxelHashMap>(mp_);
    pending_map_ = std::make_shared<VoxelHashMap>(mp_);
    if (lp_.enable_assoc_map || lp_.enable_coarse_assoc) {
      assoc_mp_ = mp_;
      assoc_mp_.voxel_size =
          lp_.enable_coarse_assoc ? (mp_.voxel_size * lp_.coarse_voxel_mul)
                                  : lp_.assoc_voxel;
      assoc_active_map_ = std::make_shared<VoxelHashMap>(assoc_mp_);
      assoc_pending_map_ = std::make_shared<VoxelHashMap>(assoc_mp_);
    }
    if (lp_.enable_async_rebuild) StartRebuildThread();
  }

  ~LocalMap() { Shutdown(); }

  void Reset() {
    Shutdown();
    std::lock_guard<std::mutex> lk(frames_mutex_);
    frames_.clear();
    {
      std::lock_guard<std::mutex> mlk(map_mutex_);
      if (active_map_) active_map_->Clear();
      if (pending_map_) pending_map_->Clear();
      if (assoc_active_map_) assoc_active_map_->Clear();
      if (assoc_pending_map_) assoc_pending_map_->Clear();
    }
    frame_count_ = 0;
    last_rebuild_ms_ = 0.0;
    last_rebuild_async_ms_ = 0.0;
    map_version_ = 0;
    last_rebuild_center_ = Eigen::Vector3d::Zero();
    map_ready_ = false;
    if (lp_.enable_async_rebuild) StartRebuildThread();
  }

  bool Empty() const { return frames_.empty(); }

  // 初始化（第一帧）
  void InitWithFrame(const std::vector<Eigen::Vector3d>& pts_w,
                     const Sophus::SE3d& T_wb) {
    Reset();
    {
      std::lock_guard<std::mutex> lk(frames_mutex_);
      frames_.push_back(Frame{pts_w, T_wb.translation()});
    }
    frame_count_ = 1;
    Rebuild(T_wb.translation());
  }

  // 增加一帧（pts_w = 这帧用于建图的世界系点）
  void AddFrame(const std::vector<Eigen::Vector3d>& pts_w,
                const Sophus::SE3d& T_wb) {
    {
      std::lock_guard<std::mutex> lk(frames_mutex_);
      frames_.push_back(Frame{pts_w, T_wb.translation()});

      while (static_cast<int>(frames_.size()) > lp_.window_size) {
        frames_.pop_front();
      }
    }

    ++frame_count_;
    if (lp_.rebuild_every_n <= 1 || (frame_count_ % lp_.rebuild_every_n) == 0) {
      if (lp_.enable_async_rebuild) {
        if (frame_count_ <= lp_.async_warmup_frames) {
          Rebuild(T_wb.translation());
        } else {
          RequestRebuild(T_wb.translation());
        }
      } else {
        Rebuild(T_wb.translation());
      }
    }
  }

  std::shared_ptr<const VoxelHashMap> GetActiveMapPtr() const {
    std::lock_guard<std::mutex> lk(map_mutex_);
    return active_map_;
  }
  std::shared_ptr<const VoxelHashMap> GetActiveAssocMapPtr() const {
    std::lock_guard<std::mutex> lk(map_mutex_);
    return assoc_active_map_ ? assoc_active_map_ : active_map_;
  }
  std::size_t GetActiveVoxelCount() const {
    std::lock_guard<std::mutex> lk(map_mutex_);
    return active_map_ ? active_map_->VoxelCount() : 0;
  }

  int window_size() const { return static_cast<int>(frames_.size()); }
  void SetPerfEnabled(bool enabled) {
    perf_enabled_ = enabled;
    std::lock_guard<std::mutex> lk(map_mutex_);
    if (active_map_) active_map_->SetPerfEnabled(enabled);
    if (pending_map_) pending_map_->SetPerfEnabled(enabled);
    if (assoc_active_map_) assoc_active_map_->SetPerfEnabled(enabled);
    if (assoc_pending_map_) assoc_pending_map_->SetPerfEnabled(enabled);
  }
  double last_rebuild_ms() const {
    std::lock_guard<std::mutex> lk(map_mutex_);
    return last_rebuild_ms_;
  }
  double last_rebuild_async_ms() const {
    std::lock_guard<std::mutex> lk(map_mutex_);
    return last_rebuild_async_ms_;
  }
  std::uint64_t map_version() const {
    std::lock_guard<std::mutex> lk(map_mutex_);
    return map_version_;
  }
  bool map_ready() const {
    std::lock_guard<std::mutex> lk(map_mutex_);
    return map_ready_;
  }

  void ResetActivePerf() {
    std::lock_guard<std::mutex> lk(map_mutex_);
    if (active_map_) active_map_->ResetPerf();
  }
  VoxelHashMap::PerfCounters GetActivePerfAndReset() {
    std::lock_guard<std::mutex> lk(map_mutex_);
    return active_map_ ? active_map_->GetPerfAndReset()
                       : VoxelHashMap::PerfCounters{};
  }
  VoxelHashMap::PerfCounters GetActiveAssocPerfAndReset() {
    std::lock_guard<std::mutex> lk(map_mutex_);
    return assoc_active_map_ ? assoc_active_map_->GetPerfAndReset()
                             : VoxelHashMap::PerfCounters{};
  }
  VoxelHashMap::StatsSummary GetActiveStatsSummaryAndReset() {
    std::lock_guard<std::mutex> lk(map_mutex_);
    return active_map_ ? active_map_->GetStatsSummaryAndReset()
                       : VoxelHashMap::StatsSummary{};
  }
  VoxelHashMap::StatsSummary GetActiveAssocStatsSummaryAndReset() {
    std::lock_guard<std::mutex> lk(map_mutex_);
    return assoc_active_map_ ? assoc_active_map_->GetStatsSummaryAndReset()
                             : VoxelHashMap::StatsSummary{};
  }
  VoxelHashMap::ScoreSummary GetActiveScoreSummary() const {
    std::lock_guard<std::mutex> lk(map_mutex_);
    return active_map_ ? active_map_->GetScoreSummary()
                       : VoxelHashMap::ScoreSummary{};
  }
  VoxelHashMap::EdgeScoreSummary GetActiveEdgeScoreSummary(double tau_edge) const {
    std::lock_guard<std::mutex> lk(map_mutex_);
    return active_map_ ? active_map_->GetEdgeScoreSummary(tau_edge)
                       : VoxelHashMap::EdgeScoreSummary{};
  }

  void MaybeSwapPending() {
    if (!pending_ready_.load()) return;
    std::lock_guard<std::mutex> lk(map_mutex_);
    if (!pending_ready_.load()) return;
    assert(active_map_.get() != pending_map_.get());
    active_map_.swap(pending_map_);
    if (assoc_active_map_ && assoc_pending_map_) {
      assert(assoc_active_map_.get() != assoc_pending_map_.get());
      assoc_active_map_.swap(assoc_pending_map_);
    }
    pending_ready_ = false;
    map_version_ += 1;
    map_ready_ = true;
  }

  bool WaitForPendingReadyAndSwap(int timeout_ms, double* waited_ms) {
    using Clock = std::chrono::steady_clock;
    const auto t0 = Clock::now();
    std::unique_lock<std::mutex> lk(map_mutex_);
    if (!pending_ready_.load()) {
      if (timeout_ms <= 0) {
        if (waited_ms) *waited_ms = 0.0;
        return false;
      }
      map_cv_.wait_for(lk, std::chrono::milliseconds(timeout_ms), [&]() {
        return pending_ready_.load();
      });
    }
    if (pending_ready_.load()) {
      assert(active_map_.get() != pending_map_.get());
      active_map_.swap(pending_map_);
      pending_ready_ = false;
      map_version_ += 1;
      if (waited_ms) {
        const auto t1 = Clock::now();
        *waited_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
      }
      return true;
    }
    if (waited_ms) {
      const auto t1 = Clock::now();
      *waited_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    }
    return false;
  }

  void Shutdown() {
    {
      std::lock_guard<std::mutex> lk(rebuild_mutex_);
      stop_thread_ = true;
      request_pending_ = false;
    }
    cv_.notify_all();
    map_cv_.notify_all();
    if (rebuild_thread_.joinable()) rebuild_thread_.join();
    stop_thread_ = false;
  }

private:
  struct Frame {
    std::vector<Eigen::Vector3d> pts_w;
    Eigen::Vector3d t_wb;  // 这一帧位姿的平移（用于可选裁剪）
  };

  void Rebuild(const Eigen::Vector3d& center_w) {
    using Clock = std::chrono::steady_clock;
    const auto t0 = perf_enabled_ ? Clock::now() : Clock::time_point{};
    {
      std::lock_guard<std::mutex> lk(map_mutex_);
      active_map_->Clear();
      if (assoc_active_map_) assoc_active_map_->Clear();
    }

    // 预估reserve（减少rehash）
    std::size_t total_pts = 0;
    for (const auto& f : frames_) total_pts += f.pts_w.size();

    // 按窗口重建
    if (!lp_.use_radius_crop) {
      // 直接全部插入
      // 为了少一次大vector拷贝，逐帧Insert即可
      {
        std::lock_guard<std::mutex> lk(map_mutex_);
        for (const auto& f : frames_) active_map_->InsertPoints(f.pts_w);
        if (assoc_active_map_) {
          for (const auto& f : frames_) assoc_active_map_->InsertPoints(f.pts_w);
        }
      }
      if (perf_enabled_) {
        const auto t1 = Clock::now();
        std::lock_guard<std::mutex> lk(map_mutex_);
        last_rebuild_ms_ =
            std::chrono::duration<double, std::milli>(t1 - t0).count();
      }
      return;
    }

    // 半径裁剪（center_w 一般取当前帧位置）
    const double r2 = lp_.crop_radius * lp_.crop_radius;
    std::vector<Eigen::Vector3d> buf;
    buf.reserve(total_pts);

    for (const auto& f : frames_) {
      for (const auto& p : f.pts_w) {
        if ((p - center_w).squaredNorm() <= r2) buf.push_back(p);
      }
    }
    {
      std::lock_guard<std::mutex> lk(map_mutex_);
      active_map_->InsertPoints(buf);
      if (assoc_active_map_) assoc_active_map_->InsertPoints(buf);
    }
    if (perf_enabled_) {
      const auto t1 = Clock::now();
      std::lock_guard<std::mutex> lk(map_mutex_);
      last_rebuild_ms_ =
          std::chrono::duration<double, std::milli>(t1 - t0).count();
    }
    {
      std::lock_guard<std::mutex> lk(map_mutex_);
      last_rebuild_center_ = center_w;
      map_version_ += 1;
      map_ready_ = true;
    }
  }

  void StartRebuildThread() {
    stop_thread_ = false;
    request_pending_ = false;
    pending_ready_ = false;
    rebuild_thread_ = std::thread([this]() { RebuildWorker(); });
  }

  void RequestRebuild(const Eigen::Vector3d& center_w) {
    if (lp_.rebuild_min_move > 0.0) {
      const double d = (center_w - last_rebuild_center_).norm();
      if (d < lp_.rebuild_min_move) return;
    }
    {
      std::lock_guard<std::mutex> lk(rebuild_mutex_);
      requested_center_ = center_w;
      request_pending_ = true;
    }
    cv_.notify_one();
  }

  void RebuildWorker() {
    for (;;) {
      Eigen::Vector3d center;
      {
        std::unique_lock<std::mutex> lk(rebuild_mutex_);
        cv_.wait(lk, [&]() { return stop_thread_ || request_pending_; });
        if (stop_thread_) return;
        center = requested_center_;
        request_pending_ = false;
      }

      const auto t0 = std::chrono::steady_clock::now();

      std::deque<Frame> frames_copy;
      {
        std::lock_guard<std::mutex> lk(frames_mutex_);
        frames_copy = frames_;
      }

      auto new_map = std::make_shared<VoxelHashMap>(mp_);
      std::shared_ptr<VoxelHashMap> new_assoc_map;
      if (lp_.enable_assoc_map || lp_.enable_coarse_assoc) {
        new_assoc_map = std::make_shared<VoxelHashMap>(assoc_mp_);
        new_assoc_map->SetPerfEnabled(perf_enabled_);
      }
      new_map->SetPerfEnabled(perf_enabled_);
      if (!lp_.use_radius_crop) {
        for (const auto& f : frames_copy) {
          new_map->InsertPoints(f.pts_w);
          if (new_assoc_map) new_assoc_map->InsertPoints(f.pts_w);
        }
      } else {
        const double r2 = lp_.crop_radius * lp_.crop_radius;
        std::vector<Eigen::Vector3d> buf;
        std::size_t total_pts = 0;
        for (const auto& f : frames_copy) total_pts += f.pts_w.size();
        buf.reserve(total_pts);
        for (const auto& f : frames_copy) {
          for (const auto& p : f.pts_w) {
            if ((p - center).squaredNorm() <= r2) buf.push_back(p);
          }
        }
        new_map->InsertPoints(buf);
        if (new_assoc_map) new_assoc_map->InsertPoints(buf);
      }

      const auto t1 = std::chrono::steady_clock::now();
      {
        std::lock_guard<std::mutex> lk(map_mutex_);
        pending_map_ = std::move(new_map);
        if (new_assoc_map) assoc_pending_map_ = std::move(new_assoc_map);
        assert(active_map_.get() != pending_map_.get());
        last_rebuild_async_ms_ =
            std::chrono::duration<double, std::milli>(t1 - t0).count();
        last_rebuild_center_ = center;
        pending_ready_ = true;
        map_cv_.notify_all();
      }
    }
  }

  VoxelMapParams mp_;
  VoxelMapParams assoc_mp_;
  LocalMapParams lp_;
  std::shared_ptr<VoxelHashMap> active_map_;
  std::shared_ptr<VoxelHashMap> pending_map_;
  std::shared_ptr<VoxelHashMap> assoc_active_map_;
  std::shared_ptr<VoxelHashMap> assoc_pending_map_;
  std::deque<Frame> frames_;
  int frame_count_ = 0;
  bool perf_enabled_ = false;
  double last_rebuild_ms_ = 0.0;
  double last_rebuild_async_ms_ = 0.0;
  std::uint64_t map_version_ = 0;
  Eigen::Vector3d last_rebuild_center_ = Eigen::Vector3d::Zero();
  bool map_ready_ = false;
  std::mutex frames_mutex_;
  mutable std::mutex map_mutex_;
  std::condition_variable map_cv_;
  std::mutex rebuild_mutex_;
  std::condition_variable cv_;
  std::thread rebuild_thread_;
  std::atomic<bool> pending_ready_{false};
  bool stop_thread_ = false;
  bool request_pending_ = false;
  Eigen::Vector3d requested_center_ = Eigen::Vector3d::Zero();
};

}  // namespace struct_icp
