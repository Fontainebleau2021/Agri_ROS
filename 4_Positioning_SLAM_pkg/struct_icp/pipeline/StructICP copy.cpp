#include "StructICP.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>

namespace fs = std::filesystem;

namespace struct_icp::pipeline {

// ---- timing helper (minimal) ----
using Clock = std::chrono::steady_clock;
static inline double ms(const Clock::time_point& t0, const Clock::time_point& t1) {
  return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

double StructICP::NowSec() {
  using clock = std::chrono::steady_clock;
  return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

StructICP::StructICP(const struct_icp::PreprocessParams& pp,
                     const struct_icp::RegistrationParams& rp,
                     const struct_icp::VoxelMapParams& mp,
                     const PipelineParams& pip)
    : pp_(pp),
      rp_(rp),
      mp_(mp),
      pip_(pip),
      pre_(pp_),
      reg_(rp_, mp_),
      local_map_(mp_,
                 struct_icp::LocalMapParams{
                     pip_.local_window_size,
                     pip_.local_use_radius_crop,
                     pip_.local_crop_radius,
                     pip_.local_rebuild_every_n}) {
  reg_.topk_K = pip_.topk_K;

  // IMPORTANT: use CsvLogger::Options (NOT CsvLoggerOptions)
  log_opt_.diag_csv_path = pip_.output_dir + "/icp_diag.csv";
  log_opt_.topk_csv_path = pip_.output_dir + "/icp_topk.csv";
  log_opt_.flush_each_write = false;
  log_opt_.precision = 8;
}

bool StructICP::EnsureDirExists(const std::string& dir) {
  if (dir.empty()) return false;
  std::error_code ec;
  if (fs::exists(dir, ec)) return true;
  return fs::create_directories(dir, ec);
}

std::vector<std::string> StructICP::ListFilesSorted(const std::string& dir,
                                                    const std::string& ext) {
  std::vector<std::string> files;
  std::error_code ec;
  if (!fs::exists(dir, ec)) return files;

  for (const auto& entry : fs::directory_iterator(dir, ec)) {
    if (ec) break;
    if (!entry.is_regular_file()) continue;
    const auto path = entry.path();
    if (path.extension().string() == ext) files.push_back(path.string());
  }
  std::sort(files.begin(), files.end());
  return files;
}

std::vector<struct_icp::PointXYZI> StructICP::LoadPCD_XYZI(const std::string& path) {
  pcl::PointCloud<pcl::PointXYZI> cloud;
  if (pcl::io::loadPCDFile(path, cloud) != 0) {
    throw std::runtime_error("Failed to load PCD: " + path);
  }

  std::vector<struct_icp::PointXYZI> out;
  out.reserve(cloud.size());
  for (const auto& p : cloud.points) {
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) continue;
    struct_icp::PointXYZI q;
    q.p = Eigen::Vector3d(p.x, p.y, p.z);
    q.intensity = p.intensity;
    out.push_back(q);
  }
  return out;
}

std::vector<Eigen::Vector3d> StructICP::TransformToWorld(
    const std::vector<struct_icp::PointXYZI>& pts_body,
    const Sophus::SE3d& T_wb) {
  std::vector<Eigen::Vector3d> out;
  out.reserve(pts_body.size());
  for (const auto& p : pts_body) out.push_back(T_wb * p.p);
  return out;
}

void StructICP::WriteTumLine(std::ofstream& ofs, double t, const Sophus::SE3d& T) {
  const Eigen::Vector3d tw = T.translation();
  const Eigen::Quaterniond q(T.so3().unit_quaternion());  // (x,y,z,w)
  ofs << t << " "
      << tw.x() << " " << tw.y() << " " << tw.z() << " "
      << q.x() << " " << q.y() << " " << q.z() << " " << q.w() << "\n";
}

bool StructICP::RunPcdDirectory() {
  if (pip_.input_dir.empty()) {
    std::cerr << "[pipeline] input_dir is empty\n";
    return false;
  }
  if (!EnsureDirExists(pip_.output_dir)) {
    std::cerr << "[pipeline] cannot create output_dir=" << pip_.output_dir << "\n";
    return false;
  }

  auto files = ListFilesSorted(pip_.input_dir, pip_.pcd_ext);
  if (files.empty()) {
    std::cerr << "[pipeline] no " << pip_.pcd_ext << " files under " << pip_.input_dir << "\n";
    return false;
  }

  // apply start/max range
  const int start = std::max(0, pip_.start_frame);
  const int end_excl = (pip_.max_frames < 0)
      ? (int)files.size()
      : std::min((int)files.size(), start + pip_.max_frames);

  if (start >= end_excl) {
    std::cerr << "[pipeline] invalid range: start=" << start << " end=" << end_excl << "\n";
    return false;
  }

  // Prepare logger/traj
  std::unique_ptr<struct_icp::CsvLogger> logger;
  const double t0 = NowSec();

  if (pip_.log_csv) {
    // CsvLogger(const Options&) + ok() + ResetT0()
    logger = std::make_unique<struct_icp::CsvLogger>(log_opt_);
    logger->ResetT0(t0);
    if (!logger->ok()) {
      std::cerr << "[pipeline] CsvLogger open failed\n";
      return false;
    }
  }

  std::ofstream tum;
  if (pip_.write_traj_tum) {
    tum.open(pip_.output_dir + "/traj.tum", std::ios::out | std::ios::trunc);
    if (!tum.good()) {
      std::cerr << "[pipeline] cannot open traj.tum\n";
      return false;
    }
  }

  // Pose state
  Sophus::SE3d T_wb = pip_.T_wb0;

  // ---- 0) init local map with first frame in range ----
  int frame0 = start;
  if (pip_.verbose) {
    std::cout << "[pipeline] init with frame " << frame0 << ": " << files[frame0] << "\n";
  }

  local_map_.Reset();

  std::vector<struct_icp::PointXYZI> raw0 = LoadPCD_XYZI(files[frame0]);

  if (pp_.enable_split_output) {
    struct_icp::PreprocessOutput po0;
    pre_.ProcessSplit(raw0, po0);

    local_map_.InitWithFrame(TransformToWorld(po0.map_points, T_wb), T_wb);

    if (pip_.verbose) {
      std::cout << "[pipeline] split_output=ON"
                << " icp_points=" << po0.icp_points.size()
                << " map_points=" << po0.map_points.size()
                << "\n";
    }
  } else {
    std::vector<struct_icp::PointXYZI> ds0;
    pre_.Process(raw0, ds0);

    local_map_.InitWithFrame(TransformToWorld(ds0, T_wb), T_wb);

    if (pip_.verbose) {
      std::cout << "[pipeline] split_output=OFF"
                << " ds=" << ds0.size()
                << " (align_pts=" << ds0.size() << " map_pts=" << ds0.size() << ")"
                << "\n";
    }
  }

  if (pip_.write_traj_tum) {
    WriteTumLine(tum, 0.0, T_wb);
  }

  // ---- 1) process subsequent frames ----
  for (int idx = frame0 + 1; idx < end_excl; ++idx) {
    const std::string& path = files[idx];
    if (pip_.verbose) {
      std::cout << "[pipeline] frame " << idx << ": " << path << "\n";
    }

    const auto t_frame0_tp = Clock::now();

    // --- LoadPCD timing ---
    const auto t_load0 = Clock::now();
    std::vector<struct_icp::PointXYZI> raw = LoadPCD_XYZI(path);
    const auto t_load1 = Clock::now();

    // --- Preprocess timing ---
    const auto t_prep0 = Clock::now();

    std::vector<struct_icp::PointXYZI> ds;   // legacy
    struct_icp::PreprocessOutput po;         // split

    const std::vector<struct_icp::PointXYZI>* align_pts = nullptr;
    const std::vector<struct_icp::PointXYZI>* map_pts   = nullptr;

    if (pp_.enable_split_output) {
      pre_.ProcessSplit(raw, po);
      align_pts = &po.icp_points;
      map_pts   = &po.map_points;

      // High-value safety: if icp_points is too small, fallback to map_points for this frame
      if (align_pts->size() < static_cast<std::size_t>(rp_.min_correspondences)) {
        align_pts = &po.map_points;
      }
    } else {
      pre_.Process(raw, ds);
      align_pts = &ds;
      map_pts   = &ds;
    }

    const auto t_prep1 = Clock::now();

    // --- Align timing ---
    std::vector<struct_icp::IcpIterStats> iter_stats;
    std::vector<struct_icp::IcpTopKStats> topk_stats;

    const auto t_align0 = Clock::now();
    const bool ok = reg_.AlignPointToVoxelMap(local_map_.map(), *align_pts, T_wb,
                                              &iter_stats, &topk_stats);
    const auto t_align1 = Clock::now();

    if (!ok) {
      std::cerr << "[pipeline] align failed at frame " << idx
                << " (align_pts=" << align_pts->size()
                << " map_pts=" << map_pts->size() << ")\n";
      return false;
    }

    // --- LogCSV timing ---
    double log_ms = 0.0;
    if (logger) {
      const auto t_log0 = Clock::now();
      const double now = t0 + (double)(idx - frame0) * pip_.frame_dt;
      for (const auto& st : iter_stats) logger->LogIcpIterStats(now, st);
      for (const auto& tk : topk_stats) logger->LogTopKStats(now, tk);
      const auto t_log1 = Clock::now();
      log_ms = ms(t_log0, t_log1);
    }

    // write trajectory
    if (pip_.write_traj_tum) {
      const double t = (double)(idx - frame0) * pip_.frame_dt;
      WriteTumLine(tum, t, T_wb);
    }

    // map insert policy
    const bool do_insert =
        pip_.insert_every_frame &&
        (pip_.insert_every_n <= 1 || ((idx - frame0) % pip_.insert_every_n == 0));

    // --- MapInsert timing ---
    double map_ms = 0.0;
    if (do_insert) {
      const auto t_map0 = Clock::now();
      local_map_.AddFrame(TransformToWorld(*map_pts, T_wb), T_wb);
      const auto t_map1 = Clock::now();
      map_ms = ms(t_map0, t_map1);
    }

    // timing print
    if (pip_.verbose) {
      const auto t_end = Clock::now();
      std::cout << "[timing] frame " << idx
                << " load=" << ms(t_load0, t_load1)
                << " prep=" << ms(t_prep0, t_prep1)
                << " align=" << ms(t_align0, t_align1)
                << " map=" << map_ms
                << " log=" << log_ms
                << " total=" << ms(t_frame0_tp, t_end)
                << " ms"
                << " (align_pts=" << align_pts->size()
                << " map_pts=" << map_pts->size() << ")"
                << "\n";
    }
  }

  if (pip_.verbose) {
    std::cout << "[pipeline] done. outputs in: " << pip_.output_dir << "\n";
    std::cout << "[pipeline] local_map window_size=" << pip_.local_window_size
              << " radius_crop=" << (pip_.local_use_radius_crop ? "ON" : "OFF")
              << " radius=" << pip_.local_crop_radius
              << " rebuild_every_n=" << pip_.local_rebuild_every_n
              << "\n";
  }
  return true;
}

}  // namespace struct_icp::pipeline
