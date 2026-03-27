#pragma once

#include <Eigen/Core>
#include <vector>
#include <cstddef>

#include <sophus/se3.hpp>

#include "Preprocessing.hpp"   // PointXYZI
#include "VoxelHashMap.hpp"    // your existing voxel map
#include "IcpStats.hpp"        // if you have it; otherwise keep your current types
// If your repo uses different header names for stats, replace accordingly.
// In your current cpp it uses IcpIterStats / IcpTopKStats and topk_K.

namespace struct_icp {

struct RegistrationParams {
  int max_iters{15};
  double max_corr_dist{1.0};
  int min_effective_corr{50};
  double eps_dx{1e-4};

  // NEW: max correspondences used per iteration (budget)
  int max_corr_per_iter{6000};

  // NEW: early-stop relative cost drop threshold (moved from cpp constant)
  double eps_cost_rel{1e-3};

  // Per-iteration yaw clamp (deg). 0 = disabled.
  double yaw_clamp_deg{0.0};

  // yaw freeze: legacy by cond(JTJ)
  bool legacy_yaw_freeze_by_cond = false;
  double yaw_info_thresh = 1e-4;

  // point-to-plane mix ratio (0 = point-to-point only)
  double alpha_point_to_plane{0.0};

  // score-filtered association (wall-only)
  bool use_score_filter = false;
  bool score_allow_fallback = true;
  double score_tau_wall = 0.6;

  // edge-first association
  bool use_edge_first = false;
  double tau_edge = 0.5;

  // query ring per tier
  bool use_ring_query = false;
  int ring_edge = 0;
  int ring_wall = 0;
  int ring_fallback = 1;

  // score-based soft weights
  bool use_score_weight = false;
  double w_base = 1.0;
  double w_score_gain = 1.0;
  double w_min = 0.5;
  double w_max = 2.0;
  bool weight_only_highscore = true;

  // corr build budget + early-stop
  bool use_corr_budget = false;
  bool corr_build_early_stop = false;
  double corr_budget_edge_ratio = 0.3;
  double corr_budget_wall_ratio = 0.5;
  double corr_budget_fallback_ratio = 0.2;

  // P2-B: landmark-first two-stage ICP
  bool enable_landmark_first = false;
  int landmark_stage_max_corr = 1500;
  int landmark_stage_min_corr = 200;
  double landmark_stage_weight = 1.0;
  int refine_stage_mode = 1;  // 0: none, 1: fallback, 2: all
  int refine_stage_max_corr = 2000;
  bool landmark_stage_use_p2plane = true;
  bool refine_stage_use_p2plane = true;

  // P3-A: single-solve weighted ICP
  bool enable_single_solve_weighted = false;
  double w_edge = 1.0;
  double w_wall = 1.0;
  double w_fallback = 1.0;
  double w_cap = 5.0;
  double w_floor = 0.1;
  bool weighted_plane = true;
  bool weighted_normalize = true;
  bool w_edge_auto = false;
  bool w_wall_auto = false;
  double w_edge_gain = 1.0;
  double w_wall_gain = 1.0;
  bool enable_easy_stop_guard = false;
  int easy_stop_min_iters = 2;
};

class Registration {
 public:
  struct AlignOptions {
    bool use_ring_query_effective = false;
    bool refine_with_fine = false;
    int refine_ring = 1;
    const VoxelHashMap* refine_map = nullptr;
    bool has_ring_fallback_override = false;
    int ring_fallback_override = 1;
    bool enable_coarse_assoc = false;
    int coarse_ring = 0;
    int coarse_refine_ring = 0;
    bool coarse_fallback_full = true;
    const VoxelHashMap* fine_map = nullptr;
  };

  struct PerfStats {
    double dt_corr_build_ms = 0.0;
    double dt_budget_ms = 0.0;
    double dt_accum_ms = 0.0;
    double dt_solve_ms = 0.0;
    std::size_t used_corr = 0;
    std::size_t query_edge_hits = 0;
    std::size_t query_wall_hits = 0;
    std::size_t query_fallback_hits = 0;
    double query_ring_used_mean = 0.0;
    std::size_t query_ring0_hits = 0;
    double ring0_hit_ratio = 0.0;
    double ring_fallback_ratio = 0.0;
    std::size_t skipped_ring0_queries = 0;
    std::size_t refine_used = 0;
    double refine_used_ratio = 0.0;
    std::size_t coarse_hits = 0;
    std::size_t coarse_refine_hits = 0;
    std::size_t coarse_fallback_hits = 0;
    int coarse_ring = 0;
    int coarse_refine_ring = 0;
  };
 
  explicit Registration(const RegistrationParams& p) : rp_(p), map_() {}

  Registration(const RegistrationParams& p, const VoxelMapParams& mp)
      : rp_(p), map_(mp) {}

  void SetMapFromPointsWorld(const std::vector<Eigen::Vector3d>& pts_w);

  bool AlignPointToVoxelMap(const std::vector<PointXYZI>& scan_body,
                            Sophus::SE3d& T_wb_io,
                            std::vector<IcpIterStats>* iter_stats = nullptr,
                            std::vector<IcpTopKStats>* topk_stats = nullptr,
                            PerfStats* perf = nullptr) const;

  bool AlignPointToVoxelMap(const VoxelHashMap& map,
                            const std::vector<PointXYZI>& scan_body,
                            Sophus::SE3d& T_wb_io,
                            std::vector<IcpIterStats>* iter_stats = nullptr,
                            std::vector<IcpTopKStats>* topk_stats = nullptr,
                            PerfStats* perf = nullptr,
                            const AlignOptions* opts = nullptr) const;
  
  int topk_K = 0;
  const RegistrationParams& params() const { return rp_; }

 private:
  RegistrationParams rp_;
  VoxelHashMap map_;
};

}  // namespace struct_icp
