#include "StructICP.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
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
                     pip_.local_rebuild_every_n,
                     pip_.enable_async_rebuild,
                     pip_.rebuild_min_move,
                     pip_.async_warmup_frames,
                     pip_.enable_assoc_map,
                     pip_.assoc_voxel,
                     pip_.enable_coarse_assoc,
                     pip_.coarse_voxel_mul}) {
  reg_.topk_K = pip_.topk_K;
  local_map_.SetPerfEnabled(pip_.enable_perf);

  // IMPORTANT: use CsvLogger::Options (NOT CsvLoggerOptions)
  log_opt_.diag_csv_path = pip_.output_dir + "/icp_diag.csv";
  log_opt_.topk_csv_path = pip_.output_dir + "/icp_topk.csv";
  log_opt_.voxel_csv_path = pip_.output_dir + "/voxel_stats.csv";
  log_opt_.voxel_score_csv_path = pip_.output_dir + "/voxel_score_stats.csv";
  log_opt_.edge_score_csv_path = pip_.output_dir + "/edge_score_stats.csv";
  if (pip_.enable_perf) {
    log_opt_.perf_csv_path = pip_.output_dir + "/perf_stats.csv";
  }
  log_opt_.flush_each_write = false;
  log_opt_.precision = 8;
  refine_ring_eff_ = pip_.coarse_refine_ring;
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
  refine_ring_eff_ = pip_.coarse_refine_ring;
  coarse_fb_hist_.clear();
  coarse_hit_hist_.clear();

  std::vector<struct_icp::PointXYZI> raw0 = LoadPCD_XYZI(files[frame0]);

  std::size_t frame0_map_points = 0;
  if (pp_.enable_split_output) {
    struct_icp::PreprocessOutput po0;
    pre_.ProcessSplit(raw0, po0);

    local_map_.InitWithFrame(TransformToWorld(po0.map_points, T_wb), T_wb);
    frame0_map_points = po0.map_points.size();

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
    frame0_map_points = ds0.size();

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

    if (pip_.enable_perf) {
      local_map_.ResetActivePerf();
    }

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
    if (align_pts->size() < static_cast<std::size_t>(rp_.min_effective_corr)) {
      align_pts = &po.map_points;
    }
    } else {
      pre_.Process(raw, ds);
      align_pts = &ds;
      map_pts   = &ds;
    }

    const auto t_prep1 = Clock::now();

    local_map_.MaybeSwapPending();
    double waited_first_map_ms = 0.0;
    int skipped_icp_due_to_small_map = 0;
    if (pip_.enable_async_rebuild && pip_.async_wait_first_map) {
      const std::size_t voxels = local_map_.GetActiveVoxelCount();
      const int min_voxels_wait =
          pip_.enable_coarse_assoc ? pip_.min_coarse_voxels_for_icp
                                   : pip_.min_voxels_for_icp;
      if (voxels < static_cast<std::size_t>(min_voxels_wait)) {
        local_map_.WaitForPendingReadyAndSwap(pip_.async_wait_timeout_ms,
                                              &waited_first_map_ms);
      }
    }
    const std::uint64_t used_map_version = local_map_.map_version();
    auto fine_map_ptr = local_map_.GetActiveMapPtr();
    auto assoc_map_ptr = local_map_.GetActiveAssocMapPtr();
    if (!fine_map_ptr || !assoc_map_ptr) {
      std::cerr << "[pipeline] active map is null at frame " << idx << "\n";
      return false;
    }
    const bool use_coarse_assoc = pip_.enable_coarse_assoc;
    const bool use_assoc_map = pip_.enable_assoc_map && !use_coarse_assoc;
    const auto& qmap =
        (use_assoc_map || use_coarse_assoc) ? *assoc_map_ptr : *fine_map_ptr;
    if (pip_.debug_map_ready && idx == frame0 + 1) {
      std::cout << "[pipeline] map_ready: voxels=" << qmap.VoxelCount()
                << " frames=" << local_map_.window_size()
                << " frame0_map_pts=" << frame0_map_points
                << " map_ready=" << (local_map_.map_ready() ? "1" : "0")
                << "\n";
    }

    const bool adaptive_enabled = pip_.enable_adaptive_ring && rp_.use_ring_query;
    const bool effective_use_ring0 =
        rp_.use_ring_query && (!adaptive_enabled || !adaptive_ring_.degraded);
    struct_icp::Registration::AlignOptions align_opts;
    align_opts.use_ring_query_effective = effective_use_ring0;
    if (use_assoc_map) {
      align_opts.has_ring_fallback_override = true;
      align_opts.ring_fallback_override = pip_.assoc_ring_fallback;
      align_opts.refine_with_fine = pip_.assoc_refine_with_fine;
      align_opts.refine_ring = pip_.assoc_refine_ring;
      align_opts.refine_map = fine_map_ptr.get();
    }
    if (use_coarse_assoc) {
      align_opts.enable_coarse_assoc = true;
      align_opts.coarse_ring = pip_.coarse_ring;
      align_opts.coarse_refine_ring =
          pip_.enable_adaptive_refine_ring ? refine_ring_eff_ : pip_.coarse_refine_ring;
      align_opts.coarse_fallback_full = pip_.coarse_fallback_full;
      align_opts.fine_map = fine_map_ptr.get();
    }

    const int adaptive_degraded_now = adaptive_ring_.degraded ? 1 : 0;
    const int adaptive_hold_left_now = adaptive_ring_.hold_left;

    // --- Align timing ---
    std::vector<struct_icp::IcpIterStats> iter_stats;
    std::vector<struct_icp::IcpTopKStats> topk_stats;
    struct_icp::Registration::PerfStats reg_perf;

    const auto t_align0 = Clock::now();
    bool ok = true;
    const int min_voxels_gate =
        use_coarse_assoc ? pip_.min_coarse_voxels_for_icp
                         : (use_assoc_map ? pip_.assoc_min_voxels_for_icp
                                          : pip_.min_voxels_for_icp);
    const bool map_small =
        (qmap.VoxelCount() < static_cast<std::size_t>(min_voxels_gate));
    const bool src_small =
        (align_pts->size() < static_cast<std::size_t>(rp_.min_effective_corr));
    if (map_small || src_small) {
      skipped_icp_due_to_small_map = 1;
    } else {
      ok = reg_.AlignPointToVoxelMap(qmap, *align_pts, T_wb,
                                     &iter_stats, &topk_stats,
                                     pip_.enable_perf ? &reg_perf : nullptr,
                                     &align_opts);
    }
    const auto t_align1 = Clock::now();

    if (!ok) {
      std::cerr << "[pipeline] align failed at frame " << idx
                << " (align_pts=" << align_pts->size()
                << " map_pts=" << map_pts->size() << ")\n";
      return false;
    }
    if (skipped_icp_due_to_small_map && pip_.verbose) {
      std::cout << "[pipeline] skip ICP (small map) at frame " << idx
                << " voxels=" << qmap.VoxelCount()
                << " align_pts=" << align_pts->size()
                << " waited_ms=" << waited_first_map_ms
                << "\n";
    }
      // ---- attach preprocess diagnostics to each ICP iter stat (重复写入方便后处理) ----
      const std::size_t n_icp = po.icp_points.size();
      const std::size_t n_map = po.map_points.size();
      auto ratio3 = [](const std::array<std::size_t,3>& b, std::size_t n){
        std::array<double,3> r{{0.0,0.0,0.0}};
        if (n == 0) return r;
        r[0] = static_cast<double>(b[0]) / static_cast<double>(n);
        r[1] = static_cast<double>(b[1]) / static_cast<double>(n);
        r[2] = static_cast<double>(b[2]) / static_cast<double>(n);
        return r;
      };
      const auto icp_rr = ratio3(po.icp_r_bins, n_icp);
      const auto map_rr = ratio3(po.map_r_bins, n_map);

      for (auto& st : iter_stats) {
        st.n_raw = po.n_raw;
        st.n_split = po.n_split;
        st.n_icp = n_icp;
        st.n_map = n_map;
        st.icp_r0 = icp_rr[0]; st.icp_r1 = icp_rr[1]; st.icp_r2 = icp_rr[2];
        st.map_r0 = map_rr[0]; st.map_r1 = map_rr[1]; st.map_r2 = map_rr[2];
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

    double coarse_fb_avg = 0.0;
    double coarse_hit_avg = 0.0;
    if (pip_.enable_coarse_assoc && !skipped_icp_due_to_small_map) {
      const std::size_t queried =
          iter_stats.empty() ? 0 : iter_stats.back().corr_queried_points;
      if (queried > 0) {
        const double fb_cur =
            static_cast<double>(reg_perf.coarse_fallback_hits) /
            static_cast<double>(queried);
        const double coarse_hit_cur =
            static_cast<double>(reg_perf.coarse_hits) /
            static_cast<double>(queried);
        coarse_fb_hist_.push_back(fb_cur);
        coarse_hit_hist_.push_back(coarse_hit_cur);
        const int win = std::max(1, pip_.adapt_window);
        while (static_cast<int>(coarse_fb_hist_.size()) > win) {
          coarse_fb_hist_.pop_front();
        }
        while (static_cast<int>(coarse_hit_hist_.size()) > win) {
          coarse_hit_hist_.pop_front();
        }
        double sum = 0.0;
        for (double v : coarse_fb_hist_) sum += v;
        coarse_fb_avg = sum / static_cast<double>(coarse_fb_hist_.size());
        sum = 0.0;
        for (double v : coarse_hit_hist_) sum += v;
        coarse_hit_avg = sum / static_cast<double>(coarse_hit_hist_.size());
      }
    }

    if (adaptive_enabled && !skipped_icp_due_to_small_map) {
      const int frame_idx = idx - frame0;
      if (frame_idx >= pip_.adaptive_ring_warmup) {
        if (!adaptive_ring_.degraded) {
          if (reg_perf.ring_fallback_ratio > pip_.tau_fallback_hi) {
            adaptive_ring_.hi_count += 1;
          } else {
            adaptive_ring_.hi_count = 0;
          }
          if (adaptive_ring_.hi_count >= pip_.fallback_hi_frames) {
            adaptive_ring_.degraded = true;
            adaptive_ring_.hold_left = pip_.degrade_hold_frames;
            adaptive_ring_.hi_count = 0;
            adaptive_ring_.lo_count = 0;
          }
        } else {
          if (adaptive_ring_.hold_left > 0) {
            adaptive_ring_.hold_left -= 1;
          } else {
            if (reg_perf.ring_fallback_ratio < pip_.tau_fallback_lo) {
              adaptive_ring_.lo_count += 1;
            } else {
              adaptive_ring_.lo_count = 0;
            }
            if (adaptive_ring_.lo_count >= pip_.fallback_lo_frames) {
              adaptive_ring_.degraded = false;
              adaptive_ring_.hi_count = 0;
              adaptive_ring_.lo_count = 0;
            }
          }
        }
      } else {
        adaptive_ring_.hi_count = 0;
        adaptive_ring_.lo_count = 0;
      }
    }

    if (pip_.enable_coarse_assoc && pip_.enable_adaptive_refine_ring &&
        !skipped_icp_due_to_small_map) {
      const int frame_idx = idx - frame0;
      const int min_ring = std::clamp(pip_.refine_ring_min, 0, 1);
      const int max_ring = std::clamp(pip_.refine_ring_max, 0, 1);
      if (frame_idx >= pip_.adapt_warmup_frames &&
          !coarse_fb_hist_.empty() && !coarse_hit_hist_.empty()) {
        const int prev = refine_ring_eff_;
        if (refine_ring_eff_ <= min_ring) {
          if (coarse_fb_avg > pip_.fallback_ratio_hi ||
              coarse_hit_avg < pip_.coarse_hit_ratio_lo) {
            refine_ring_eff_ = max_ring;
          }
        } else if (refine_ring_eff_ >= max_ring) {
          if (coarse_fb_avg < pip_.fallback_ratio_lo &&
              coarse_hit_avg >= pip_.coarse_hit_ratio_lo) {
            refine_ring_eff_ = min_ring;
          }
        }
        if (pip_.verbose && prev != refine_ring_eff_) {
          std::cout << "[adaptive_refine] frame=" << idx
                    << " fb_avg=" << coarse_fb_avg
                    << " coarse_hit_avg=" << coarse_hit_avg
                    << " refine_ring:" << prev << "->" << refine_ring_eff_
                    << "\n";
        }
      }
    }

    if (!iter_stats.empty() && pip_.verbose) {
      const auto& st_last = iter_stats.back();
      if (st_last.corr_budget_downgrade == 1) {
        std::cout << "[pipeline] corr_budget downgrade at frame " << idx
                  << " iter=" << st_last.it
                  << " used_corr=" << st_last.used_corr_total
                  << "\n";
      }
    }

    if (logger && do_insert) {
      const double now = t0 + (double)(idx - frame0) * pip_.frame_dt;
      auto st = local_map_.GetActiveStatsSummaryAndReset();
      logger->LogVoxelStats(now, st.voxel_count, st.mean_N, st.p90_N, st.eig_updated_voxels);
      auto sst = local_map_.GetActiveScoreSummary();
      logger->LogVoxelScoreStats(now,
                                 sst.voxel_count,
                                 sst.score_valid_ratio,
                                 sst.score_mean,
                                 sst.score_p90,
                                 sst.score_p99,
                                 sst.high_score_ratio,
                                 sst.planarity_mean,
                                 sst.linearity_mean,
                                 sst.verticality_mean,
                                 sst.temp_var_mean);
      auto est = local_map_.GetActiveEdgeScoreSummary(rp_.tau_edge);
      logger->LogEdgeScoreStats(now,
                                est.voxel_count,
                                est.edge_valid_ratio,
                                est.edge_score_mean,
                                est.edge_score_p90,
                                est.edge_score_p99,
                                est.high_edge_ratio);
      if (pip_.enable_perf) {
        const auto perf = local_map_.GetActivePerfAndReset();
        const double dt_load = ms(t_load0, t_load1);
        const double dt_preprocess = ms(t_prep0, t_prep1);
        const double dt_icp_total = ms(t_align0, t_align1);
        const double dt_map_update = map_ms;
        const double dt_rebuild = local_map_.last_rebuild_ms();
        double corr_edge_ratio = 0.0;
        double corr_highscore_ratio = 0.0;
        double fallback_ratio = 0.0;
        int yaw_frozen = 0;
        double yaw_info = 0.0;
        int corr_budget_downgrade = 0;
        int ring_query_downgrade = 0;
        int use_corr_budget = 0;
        int corr_build_early_stop = 0;
        int budget_edge = 0;
        int budget_wall = 0;
        int budget_fallback_eff = 0;
        std::size_t corr_edge_used = 0;
        std::size_t corr_wall_used = 0;
        std::size_t corr_fallback_used = 0;
        std::size_t used_corr_total = 0;
        std::size_t corr_queried_points = 0;
        int lm_first_enabled = rp_.enable_landmark_first ? 1 : 0;
        double lm_stage_used_ratio = 0.0;
        double lm_corr_used_mean = 0.0;
        double refine_corr_used_mean = 0.0;
        double lm_dx_norm_mean = 0.0;
        double refine_dx_norm_mean = 0.0;
        int weighted_enabled = rp_.enable_single_solve_weighted ? 1 : 0;
        double w_mean_frame = 0.0;
        double w_max_frame = 0.0;
        double edge_ratio_frame = 0.0;
        double wall_ratio_frame = 0.0;
        double fallback_ratio_frame = 0.0;
        int coarse_enabled = use_coarse_assoc ? 1 : 0;
        double coarse_hit_ratio = 0.0;
        double coarse_refine_hit_ratio = 0.0;
        double coarse_fallback_ratio = 0.0;
        int coarse_ring_used = 0;
        int refine_ring_used = 0;
        if (!iter_stats.empty()) {
          const auto& st_last = iter_stats.back();
          corr_edge_ratio = st_last.corr_edge_ratio;
          corr_highscore_ratio = st_last.corr_highscore_ratio;
          fallback_ratio = st_last.edge_first_used ? st_last.corr_fallback_ratio
                                                   : st_last.highscore_fallback_ratio;
          yaw_frozen = st_last.yaw_frozen;
          yaw_info = st_last.yaw_info;
          corr_budget_downgrade = st_last.corr_budget_downgrade;
          ring_query_downgrade = st_last.ring_query_downgrade;
          use_corr_budget = rp_.use_corr_budget ? 1 : 0;
          corr_build_early_stop = st_last.corr_build_early_stop;
          budget_edge = st_last.budget_edge;
          budget_wall = st_last.budget_wall;
          budget_fallback_eff = st_last.budget_fallback_eff;
          corr_edge_used = st_last.corr_edge_used;
          corr_wall_used = st_last.corr_wall_used;
          corr_fallback_used = st_last.corr_fallback_used;
          used_corr_total = st_last.used_corr_total;
          corr_queried_points = st_last.corr_queried_points;
          int lm_used_count = 0;
          double sum_lm_corr = 0.0;
          double sum_refine_corr = 0.0;
          double sum_lm_dx = 0.0;
          double sum_refine_dx = 0.0;
          double sum_w_mean = 0.0;
          double max_w = 0.0;
          double sum_edge_ratio = 0.0;
          double sum_wall_ratio = 0.0;
          double sum_fallback_ratio = 0.0;
          for (const auto& st : iter_stats) {
            lm_used_count += st.lm_stage_used ? 1 : 0;
            sum_lm_corr += static_cast<double>(st.lm_corr_used);
            sum_refine_corr += static_cast<double>(st.refine_corr_used);
            sum_lm_dx += st.lm_dx_norm;
            sum_refine_dx += st.refine_dx_norm;
            sum_w_mean += st.w_mean_used;
            max_w = std::max(max_w, st.w_max_used);
            if (st.used_corr_total > 0) {
              sum_edge_ratio +=
                  static_cast<double>(st.corr_edge_used) /
                  static_cast<double>(st.used_corr_total);
              sum_wall_ratio +=
                  static_cast<double>(st.corr_wall_used) /
                  static_cast<double>(st.used_corr_total);
              sum_fallback_ratio +=
                  static_cast<double>(st.corr_fallback_used) /
                  static_cast<double>(st.used_corr_total);
            }
          }
          const double denom = static_cast<double>(iter_stats.size());
          lm_stage_used_ratio = (denom > 0.0) ? (static_cast<double>(lm_used_count) / denom) : 0.0;
          lm_corr_used_mean = (denom > 0.0) ? (sum_lm_corr / denom) : 0.0;
          refine_corr_used_mean = (denom > 0.0) ? (sum_refine_corr / denom) : 0.0;
          lm_dx_norm_mean = (denom > 0.0) ? (sum_lm_dx / denom) : 0.0;
          refine_dx_norm_mean = (denom > 0.0) ? (sum_refine_dx / denom) : 0.0;
          w_mean_frame = (denom > 0.0) ? (sum_w_mean / denom) : 0.0;
          w_max_frame = max_w;
          edge_ratio_frame = (denom > 0.0) ? (sum_edge_ratio / denom) : 0.0;
          wall_ratio_frame = (denom > 0.0) ? (sum_wall_ratio / denom) : 0.0;
          fallback_ratio_frame = (denom > 0.0) ? (sum_fallback_ratio / denom) : 0.0;
        }
        if (corr_queried_points > 0) {
          coarse_hit_ratio =
              static_cast<double>(reg_perf.coarse_hits) /
              static_cast<double>(corr_queried_points);
          coarse_refine_hit_ratio =
              static_cast<double>(reg_perf.coarse_refine_hits) /
              static_cast<double>(corr_queried_points);
          coarse_fallback_ratio =
              static_cast<double>(reg_perf.coarse_fallback_hits) /
              static_cast<double>(corr_queried_points);
        }
        coarse_ring_used = reg_perf.coarse_ring;
        refine_ring_used = reg_perf.coarse_refine_ring;
        logger->LogPerfStats(now,
                             dt_load,
                             dt_preprocess,
                             dt_icp_total,
                             reg_perf.dt_corr_build_ms,
                             perf.dt_query_ms,
                             reg_perf.dt_budget_ms,
                             reg_perf.dt_accum_ms,
                             reg_perf.dt_solve_ms,
                             dt_map_update,
                             dt_rebuild,
                             local_map_.last_rebuild_async_ms(),
                             log_ms,
                             reg_perf.used_corr,
                             corr_edge_ratio,
                             corr_highscore_ratio,
                             fallback_ratio,
                             yaw_frozen,
                             yaw_info,
                             waited_first_map_ms,
                             skipped_icp_due_to_small_map,
                             reg_perf.query_edge_hits,
                             reg_perf.query_wall_hits,
                             reg_perf.query_fallback_hits,
                             reg_perf.query_ring_used_mean,
                             corr_budget_downgrade,
                             ring_query_downgrade,
                             reg_perf.ring0_hit_ratio,
                             reg_perf.ring_fallback_ratio,
                             adaptive_enabled ? 1 : 0,
                             adaptive_degraded_now,
                             adaptive_hold_left_now,
                             effective_use_ring0 ? 1 : 0,
                             reg_perf.skipped_ring0_queries,
                             use_assoc_map ? 1 : 0,
                             use_assoc_map ? pip_.assoc_voxel : 0.0,
                             (use_assoc_map && pip_.assoc_refine_with_fine) ? 1 : 0,
                             reg_perf.ring0_hit_ratio,
                             reg_perf.ring_fallback_ratio,
                             reg_perf.refine_used_ratio,
                             lm_first_enabled,
                             lm_stage_used_ratio,
                             lm_corr_used_mean,
                             refine_corr_used_mean,
                             lm_dx_norm_mean,
                             refine_dx_norm_mean,
                             use_corr_budget,
                             corr_build_early_stop,
                             budget_edge,
                             budget_wall,
                             budget_fallback_eff,
                             corr_edge_used,
                             corr_wall_used,
                             corr_fallback_used,
                             used_corr_total,
                             corr_queried_points,
                             weighted_enabled,
                             w_mean_frame,
                             w_max_frame,
                             edge_ratio_frame,
                             wall_ratio_frame,
                             fallback_ratio_frame,
                             coarse_enabled,
                             coarse_hit_ratio,
                             coarse_refine_hit_ratio,
                             coarse_fallback_ratio,
                             coarse_ring_used,
                             refine_ring_used,
                             coarse_fb_avg,
                             coarse_hit_avg,
                             local_map_.map_version(),
                             used_map_version);
      }
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
