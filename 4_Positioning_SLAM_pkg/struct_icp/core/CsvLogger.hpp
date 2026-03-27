#pragma once

#include <Eigen/Core>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include "IcpStats.hpp"

namespace struct_icp {

class CsvLogger {
public:
  struct Options {
    std::string diag_csv_path;   // e.g. ".../icp_diag.csv"
    std::string topk_csv_path;   // e.g. ".../icp_topk.csv"
    std::string voxel_csv_path;  // e.g. ".../voxel_stats.csv"
    std::string voxel_score_csv_path;  // e.g. ".../voxel_score_stats.csv"
    std::string edge_score_csv_path;   // e.g. ".../edge_score_stats.csv"
    std::string perf_csv_path;         // e.g. ".../perf_stats.csv"
    bool flush_each_write = false;
    int precision = 8;           // same as your code uses fixed+setprecision(8)
  };

  explicit CsvLogger(const Options& opt);
  ~CsvLogger();

  CsvLogger(const CsvLogger&) = delete;
  CsvLogger& operator=(const CsvLogger&) = delete;

  // 直接写一条迭代统计（对应 icp_diag.csv 一行）
  void LogIcpIterStats(double current_time_sec, const IcpIterStats& st);

  // 直接写一轮的 Top-K（对应 icp_topk.csv 多行）
  void LogTopKStats(double current_time_sec, const IcpTopKStats& st);

  // 直接写一条体素统计（对应 voxel_stats.csv 一行）
  void LogVoxelStats(double current_time_sec,
                     std::size_t voxel_count,
                     double mean_N,
                     int p90_N,
                     std::size_t eig_updated_voxels);

  void LogVoxelScoreStats(double current_time_sec,
                          std::size_t voxel_count,
                          double score_valid_ratio,
                          double score_mean,
                          double score_p90,
                          double score_p99,
                          double high_score_ratio,
                          double planarity_mean,
                          double linearity_mean,
                          double verticality_mean,
                          double temp_var_mean);

  void LogEdgeScoreStats(double current_time_sec,
                         std::size_t voxel_count,
                         double edge_valid_ratio,
                         double edge_score_mean,
                         double edge_score_p90,
                         double edge_score_p99,
                         double high_edge_ratio);

  void LogPerfStats(double current_time_sec,
                    double dt_load,
                    double dt_preprocess,
                    double dt_icp_total,
                    double dt_corr_build,
                    double dt_query,
                    double dt_budget,
                    double dt_accum,
                    double dt_solve,
                    double dt_map_update,
                    double dt_rebuild,
                    double dt_rebuild_async,
                    double dt_logging,
                    std::size_t used_corr,
                    double corr_edge_ratio,
                    double corr_highscore_ratio,
                    double fallback_ratio,
                    int yaw_frozen,
                    double yaw_info,
                    double waited_first_map_ms,
                    int skipped_icp_due_to_small_map,
                    std::size_t query_edge_hits,
                    std::size_t query_wall_hits,
                    std::size_t query_fallback_hits,
                    double query_ring_used_mean,
                    int corr_budget_downgrade,
                    int ring_query_downgrade,
                    double ring0_hit_ratio,
                    double ring_fallback_ratio,
                    int adaptive_ring_enabled,
                    int adaptive_ring_degraded,
                    int adaptive_ring_hold_left,
                    int effective_use_ring0,
                    std::size_t skipped_ring0_queries,
                    int assoc_map_enabled,
                    double assoc_voxel,
                    int assoc_refine_with_fine,
                    double assoc_ring0_hit_ratio,
                    double assoc_fallback_ratio,
                    double refine_used_ratio,
                    int lm_first_enabled,
                    double lm_stage_used_ratio,
                    double lm_corr_used_mean,
                    double refine_corr_used_mean,
                    double lm_dx_norm_mean,
                    double refine_dx_norm_mean,
                    int use_corr_budget,
                    int corr_build_early_stop,
                    int budget_edge,
                    int budget_wall,
                    int budget_fallback_eff,
                    std::size_t corr_edge_used,
                    std::size_t corr_wall_used,
                    std::size_t corr_fallback_used,
                    std::size_t used_corr_total,
                    std::size_t corr_queried_points,
                    int weighted_enabled,
                    double w_mean_frame,
                    double w_max_frame,
                    double edge_ratio,
                    double wall_ratio,
                    double fallback_ratio_frame,
                    int coarse_enabled,
                    double coarse_hit_ratio,
                    double coarse_refine_hit_ratio,
                    double coarse_fallback_ratio,
                    int coarse_ring,
                    int refine_ring,
                    double fb_avg,
                    double coarse_hit_avg,
                    std::uint64_t map_version,
                    std::uint64_t used_map_version);

  // ---- time handling ----
  // Call this once per session if你希望 t_rel=0 从某个时刻开始
  void ResetT0(double t0_sec);
  // If you don't call ResetT0, first Log* call will set t0 automatically

  // ---- ICP diag (per-iteration) ----
  // Header (exactly):
  // t_rel,it,N,inlier_ratio,mean_res,median_res,mad_res,cond_JTJ,grad_norm,
  // dtrans,dyaw,droll,dpitch,yaw_raw,yaw_clamped,yaw_clamp_triggered,yaw_frozen,
  // eig_min,eig_max,eig_rank,num_corr,corr_p50,corr_p90,n_raw,n_split,
  // n_icp,n_map,icp_r0,icp_r1,icp_r2,map_r0,map_r1,map_r2,
  // alpha_p2plane,plane_used_ratio,plane_res_mean,
  // corr_highscore_ratio,highscore_fallback_ratio,score_tau_wall,score_filter_used,
  // w_mean,w_p90,w_min,w_max,w_highscore_ratio,score_mean_used,
  // corr_edge_ratio,corr_wall_ratio,corr_fallback_ratio,tau_edge,edge_first_used,
  // yaw_info,yaw_freeze_reason,cond_JTJ_scaled
  void LogIcpDiag(double current_time_sec,
                  int it,
                  std::size_t N,
                  double inlier_ratio,
                  double mean_res,
                  double median_res,
                  double mad_res,
                  double cond_JTJ,
                  double grad_norm);

  // ---- Top-K points (per-iteration) ----
  // Header (exactly):
  // t_rel,it,idx,px,py,pz,qx,qy,qz,influence,weight
  void LogTopK(double current_time_sec,
               int it,
               const std::vector<Eigen::Vector3d>& src_topK,
               const std::vector<Eigen::Vector3d>& tgt_topK,
               const std::vector<double>& influence_topK,
               const std::vector<double>& weight_topK);

  bool ok() const { return ok_; }

private:
  Options opt_;
  std::ofstream diag_;
  std::ofstream topk_;
  std::ofstream voxel_;
  std::ofstream voxel_score_;
  std::ofstream edge_score_;
  std::ofstream perf_;
  bool ok_ = false;

  bool has_t0_ = false;
  double t0_ = 0.0;
  bool diag_header_written_ = false;
  bool topk_header_written_ = false;
  bool voxel_header_written_ = false;
  bool voxel_score_header_written_ = false;
  bool edge_score_header_written_ = false;
  bool perf_header_written_ = false;

  void WriteHeadersIfNeeded();
  double RelTime(double current_time_sec);

  static void SetStreamFormat(std::ofstream& ofs, int precision);
};

}  // namespace struct_icp
