#pragma once
#include <Eigen/Core>
#include <vector>
#include <cstddef>

namespace struct_icp {

// 对应 icp_diag.csv 一行（每次 ICP 迭代一行；与 t_rel 一起定位到某一帧）
struct IcpIterStats {
  int it = 0;
  std::size_t N = 0;              // 本次迭代参与优化的有效对应数/点数（实现里以 correspondences 数为准）
  double inlier_ratio = 0.0;

  // 残差分布（以对应距离为单位，通常是米）
  double mean_res = 0.0;
  double median_res = 0.0;
  double mad_res = 0.0;

  // 数值稳定性
  double cond_JTJ = 0.0;
  double grad_norm = 0.0;

  // --- 新增：用于判断“偏航漂还是平移漂” ---
  // 本帧 ICP 的最终相对增量（从初始 guess 到最终结果的相对位姿）
  // 注意：单位为米 / 弧度
  double dtrans = 0.0;   // ||Δt||
  double dyaw   = 0.0;   // yaw(z)
  double droll  = 0.0;   // roll(x)
  double dpitch = 0.0;   // pitch(y)

  // --- 新增：yaw clamp 诊断 ---
  double yaw_raw = 0.0;
  double yaw_clamped = 0.0;
  int yaw_clamp_triggered = 0;
  int yaw_frozen = 0;

  // --- 新增：JTJ 特征值谱摘要 ---
  double eig_min = 0.0;
  double eig_max = 0.0;
  int eig_rank   = 0;    // 有效秩（阈值见实现）

  // --- 新增：对应距离分布（P50/P90） ---
  std::size_t num_corr = 0;
  double corr_p50 = 0.0;
  double corr_p90 = 0.0;

  // --- 新增：split 前后点数 + 桶占比（来自 PreprocessOutput；会在每次迭代重复写入） ---
  std::size_t n_raw = 0;
  std::size_t n_split = 0;     // split/滤波后的临时点数（进入 voxel 的输入）
  std::size_t n_icp = 0;
  std::size_t n_map = 0;

  // 以半径 r 分 3 桶：near/mid/far（实现里默认阈值 5m / 15m，可按需改）
  double icp_r0 = 0.0;
  double icp_r1 = 0.0;
  double icp_r2 = 0.0;
  double map_r0 = 0.0;
  double map_r1 = 0.0;
  double map_r2 = 0.0;

  // --- 新增：point-to-plane diagnostics ---
  double alpha_p2plane = 0.0;
  double plane_used_ratio = 0.0;
  double plane_res_mean = 0.0;

  // --- 新增：score-filtered association diagnostics ---
  double corr_highscore_ratio = 0.0;
  double highscore_fallback_ratio = 0.0;
  double score_tau_wall = 0.0;
  int score_filter_used = 0;

  // --- 新增：score-weight diagnostics ---
  double w_mean = 0.0;
  double w_p90 = 0.0;
  double w_min = 0.0;
  double w_max = 0.0;
  double w_highscore_ratio = 0.0;
  double score_mean_used = 0.0;

  // --- 新增：edge-first association diagnostics ---
  double corr_edge_ratio = 0.0;
  double corr_wall_ratio = 0.0;
  double corr_fallback_ratio = 0.0;
  double tau_edge = 0.0;
  int edge_first_used = 0;

  // --- 新增：yaw observability diagnostics ---
  double yaw_info = 0.0;
  int yaw_freeze_reason = 0;  // 0 none, 1 low_info, 2 legacy_cond
  double cond_JTJ_scaled = 0.0;

  // --- 新增：corr build diagnostics ---
  int budget_edge = 0;
  int budget_wall = 0;
  int budget_fallback_eff = 0;
  std::size_t used_corr_total = 0;
  std::size_t corr_edge_used = 0;
  std::size_t corr_wall_used = 0;
  std::size_t corr_fallback_used = 0;
  int corr_build_early_stop = 0;
  std::size_t corr_queried_points = 0;
  int corr_budget_downgrade = 0;
  int ring_query_downgrade = 0;
  int lm_first_enabled = 0;
  int lm_stage_used = 0;
  int lm_corr_used = 0;
  int refine_stage_mode = 0;
  int refine_corr_used = 0;
  double lm_dx_norm = 0.0;
  double refine_dx_norm = 0.0;
  double lm_cost = 0.0;
  double refine_cost = 0.0;
  double lm_plane_used_ratio = 0.0;
  double refine_plane_used_ratio = 0.0;

  // --- P3-A: single-solve weighted ICP diagnostics ---
  int weighted_enabled = 0;
  double w_edge = 0.0;
  double w_wall = 0.0;
  double w_fallback = 0.0;
  double w_cap = 0.0;
  double w_mean_used = 0.0;
  double w_min_used = 0.0;
  double w_max_used = 0.0;
  double w_edge_used_mean = 0.0;
  double w_wall_used_mean = 0.0;
  double w_fallback_used_mean = 0.0;
  double cost_u = 0.0;
  double rel_drop_u = 0.0;
  int stop_by_cost_u = 0;
};

// 对应 icp_topk.csv 的一批行（同一迭代 it）
struct IcpTopKStats {
  int it = 0;
  std::vector<Eigen::Vector3d> src_pts;   // p
  std::vector<Eigen::Vector3d> tgt_pts;   // q
  std::vector<double> influence;
  std::vector<double> weight;
};

} // namespace struct_icp
