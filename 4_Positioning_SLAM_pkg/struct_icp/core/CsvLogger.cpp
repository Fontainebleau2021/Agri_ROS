#include "CsvLogger.hpp"

#include <algorithm>
#include <iomanip>
#include <stdexcept>

namespace struct_icp {

void CsvLogger::LogIcpIterStats(double current_time_sec, const IcpIterStats& st) {
  const double t_rel = RelTime(current_time_sec);

  if (!diag_.is_open()) return;

  WriteHeadersIfNeeded();

  // 注意：这里一行对应一次 ICP 迭代（同一帧会有多行）
  diag_ << std::fixed << std::setprecision(8)
        << t_rel << ","
        << st.it << ","
        << st.N << ","
        << st.inlier_ratio << ","
        << st.mean_res << ","
        << st.median_res << ","
        << st.mad_res << ","
        << st.cond_JTJ << ","
        << st.grad_norm << ","
        // Δpose
        << st.dtrans << ","
        << st.dyaw << ","
        << st.droll << ","
        << st.dpitch << ","
        << st.yaw_raw << ","
        << st.yaw_clamped << ","
        << st.yaw_clamp_triggered << ","
        << st.yaw_frozen << ","
        // JTJ eigen spectrum
        << st.eig_min << ","
        << st.eig_max << ","
        << st.eig_rank << ","
        // correspondences
        << st.num_corr << ","
        << st.corr_p50 << ","
        << st.corr_p90 << ","
        // split stats
        << st.n_raw << ","
        << st.n_split << ","
        << st.n_icp << ","
        << st.n_map << ","
        // radial buckets ratios
        << st.icp_r0 << ","
        << st.icp_r1 << ","
        << st.icp_r2 << ","
        << st.map_r0 << ","
        << st.map_r1 << ","
        << st.map_r2 << ","
        // p2plane diagnostics
        << st.alpha_p2plane << ","
        << st.plane_used_ratio << ","
        << st.plane_res_mean << ","
        // score-filtered diagnostics
        << st.corr_highscore_ratio << ","
        << st.highscore_fallback_ratio << ","
        << st.score_tau_wall << ","
        << st.score_filter_used << ","
        // score-weight diagnostics
        << st.w_mean << ","
        << st.w_p90 << ","
        << st.w_min << ","
        << st.w_max << ","
        << st.w_highscore_ratio << ","
        << st.score_mean_used << ","
        // edge-first diagnostics
        << st.corr_edge_ratio << ","
        << st.corr_wall_ratio << ","
        << st.corr_fallback_ratio << ","
        << st.tau_edge << ","
        << st.edge_first_used << ","
        // yaw observability diagnostics
        << st.yaw_info << ","
        << st.yaw_freeze_reason << ","
        << st.cond_JTJ_scaled << ","
        << st.budget_edge << ","
        << st.budget_wall << ","
        << st.budget_fallback_eff << ","
        << st.used_corr_total << ","
        << st.corr_edge_used << ","
        << st.corr_wall_used << ","
        << st.corr_fallback_used << ","
        << st.corr_build_early_stop << ","
        << st.corr_queried_points << ","
        << st.corr_budget_downgrade << ","
        << st.ring_query_downgrade << ","
        << st.lm_first_enabled << ","
        << st.lm_stage_used << ","
        << st.lm_corr_used << ","
        << st.refine_stage_mode << ","
        << st.refine_corr_used << ","
        << st.lm_dx_norm << ","
        << st.refine_dx_norm << ","
        << st.lm_cost << ","
        << st.refine_cost << ","
        << st.lm_plane_used_ratio << ","
        << st.refine_plane_used_ratio << ","
        << st.weighted_enabled << ","
        << st.w_edge << ","
        << st.w_wall << ","
        << st.w_fallback << ","
        << st.w_cap << ","
        << st.w_mean_used << ","
        << st.w_min_used << ","
        << st.w_max_used << ","
        << st.w_edge_used_mean << ","
        << st.w_wall_used_mean << ","
        << st.w_fallback_used_mean << ","
        << st.cost_u << ","
        << st.rel_drop_u << ","
        << st.stop_by_cost_u
        << "\n";

  if (opt_.flush_each_write) diag_.flush();
}

void CsvLogger::LogTopKStats(double current_time_sec, const IcpTopKStats& st) {
  // 对齐字段：t_rel,it,idx,px,py,pz,qx,qy,qz,influence,weight
  LogTopK(current_time_sec,
          st.it,
          st.src_pts,
          st.tgt_pts,
          st.influence,
          st.weight);
}

void CsvLogger::LogVoxelStats(double current_time_sec,
                              std::size_t voxel_count,
                              double mean_N,
                              int p90_N,
                              std::size_t eig_updated_voxels) {
  if (!voxel_.is_open()) return;

  const double t_rel = RelTime(current_time_sec);
  WriteHeadersIfNeeded();

  voxel_ << std::fixed << std::setprecision(8)
         << t_rel << ","
         << voxel_count << ","
         << mean_N << ","
         << p90_N << ","
         << eig_updated_voxels
         << "\n";

  if (opt_.flush_each_write) voxel_.flush();
}

void CsvLogger::LogVoxelScoreStats(double current_time_sec,
                                   std::size_t voxel_count,
                                   double score_valid_ratio,
                                   double score_mean,
                                   double score_p90,
                                   double score_p99,
                                   double high_score_ratio,
                                   double planarity_mean,
                                   double linearity_mean,
                                   double verticality_mean,
                                   double temp_var_mean) {
  if (!voxel_score_.is_open()) return;

  const double t_rel = RelTime(current_time_sec);
  WriteHeadersIfNeeded();

  voxel_score_ << std::fixed << std::setprecision(8)
               << t_rel << ","
               << voxel_count << ","
               << score_valid_ratio << ","
               << score_mean << ","
               << score_p90 << ","
               << score_p99 << ","
               << high_score_ratio << ","
               << planarity_mean << ","
               << linearity_mean << ","
               << verticality_mean << ","
               << temp_var_mean
               << "\n";

  if (opt_.flush_each_write) voxel_score_.flush();
}

void CsvLogger::LogEdgeScoreStats(double current_time_sec,
                                  std::size_t voxel_count,
                                  double edge_valid_ratio,
                                  double edge_score_mean,
                                  double edge_score_p90,
                                  double edge_score_p99,
                                  double high_edge_ratio) {
  if (!edge_score_.is_open()) return;

  const double t_rel = RelTime(current_time_sec);
  WriteHeadersIfNeeded();

  edge_score_ << std::fixed << std::setprecision(8)
              << t_rel << ","
              << voxel_count << ","
              << edge_valid_ratio << ","
              << edge_score_mean << ","
              << edge_score_p90 << ","
              << edge_score_p99 << ","
              << high_edge_ratio
              << "\n";

  if (opt_.flush_each_write) edge_score_.flush();
}

void CsvLogger::LogPerfStats(double current_time_sec,
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
                             std::uint64_t used_map_version) {
  if (!perf_.is_open()) return;

  const double t_rel = RelTime(current_time_sec);
  WriteHeadersIfNeeded();

  perf_ << std::fixed << std::setprecision(8)
        << t_rel << ","
        << dt_load << ","
        << dt_preprocess << ","
        << dt_icp_total << ","
        << dt_corr_build << ","
        << dt_query << ","
        << dt_budget << ","
        << dt_accum << ","
        << dt_solve << ","
        << dt_map_update << ","
        << dt_rebuild << ","
        << dt_rebuild_async << ","
        << dt_logging << ","
        << used_corr << ","
        << corr_edge_ratio << ","
        << corr_highscore_ratio << ","
        << fallback_ratio << ","
        << yaw_frozen << ","
        << yaw_info << ","
        << waited_first_map_ms << ","
        << skipped_icp_due_to_small_map << ","
        << query_edge_hits << ","
        << query_wall_hits << ","
        << query_fallback_hits << ","
        << query_ring_used_mean << ","
        << corr_budget_downgrade << ","
        << ring_query_downgrade << ","
        << ring0_hit_ratio << ","
        << ring_fallback_ratio << ","
        << adaptive_ring_enabled << ","
        << adaptive_ring_degraded << ","
        << adaptive_ring_hold_left << ","
        << effective_use_ring0 << ","
        << skipped_ring0_queries << ","
        << assoc_map_enabled << ","
        << assoc_voxel << ","
        << assoc_refine_with_fine << ","
        << assoc_ring0_hit_ratio << ","
        << assoc_fallback_ratio << ","
        << refine_used_ratio << ","
        << lm_first_enabled << ","
        << lm_stage_used_ratio << ","
        << lm_corr_used_mean << ","
        << refine_corr_used_mean << ","
        << lm_dx_norm_mean << ","
        << refine_dx_norm_mean << ","
        << use_corr_budget << ","
        << corr_build_early_stop << ","
        << budget_edge << ","
        << budget_wall << ","
        << budget_fallback_eff << ","
        << corr_edge_used << ","
        << corr_wall_used << ","
        << corr_fallback_used << ","
        << used_corr_total << ","
        << corr_queried_points << ","
        << weighted_enabled << ","
        << w_mean_frame << ","
        << w_max_frame << ","
        << edge_ratio << ","
        << wall_ratio << ","
        << fallback_ratio_frame << ","
        << coarse_enabled << ","
        << coarse_hit_ratio << ","
        << coarse_refine_hit_ratio << ","
        << coarse_fallback_ratio << ","
        << coarse_ring << ","
        << refine_ring << ","
        << fb_avg << ","
        << coarse_hit_avg << ","
        << map_version << ","
        << used_map_version
        << "\n";

  if (opt_.flush_each_write) perf_.flush();
}

static bool IsPathEmpty(const std::string& s) { return s.empty(); }

CsvLogger::CsvLogger(const Options& opt) : opt_(opt) {
  diag_header_written_ = false;
  topk_header_written_ = false;
  voxel_header_written_ = false;
  voxel_score_header_written_ = false;
  edge_score_header_written_ = false;
  perf_header_written_ = false;
  // Open files if paths provided
  if (!IsPathEmpty(opt_.diag_csv_path)) {
    diag_.open(opt_.diag_csv_path, std::ios::out | std::ios::trunc);
  }
  if (!IsPathEmpty(opt_.topk_csv_path)) {
    topk_.open(opt_.topk_csv_path, std::ios::out | std::ios::trunc);
  }
  if (!IsPathEmpty(opt_.voxel_csv_path)) {
    voxel_.open(opt_.voxel_csv_path, std::ios::out | std::ios::trunc);
  }
  if (!IsPathEmpty(opt_.voxel_score_csv_path)) {
    voxel_score_.open(opt_.voxel_score_csv_path, std::ios::out | std::ios::trunc);
  }
  if (!IsPathEmpty(opt_.edge_score_csv_path)) {
    edge_score_.open(opt_.edge_score_csv_path, std::ios::out | std::ios::trunc);
  }
  if (!IsPathEmpty(opt_.perf_csv_path)) {
    perf_.open(opt_.perf_csv_path, std::ios::out | std::ios::trunc);
  }

  ok_ = (!diag_.is_open() && IsPathEmpty(opt_.diag_csv_path) ? true : diag_.good()) &&
        (!topk_.is_open() && IsPathEmpty(opt_.topk_csv_path) ? true : topk_.good()) &&
        (!voxel_.is_open() && IsPathEmpty(opt_.voxel_csv_path) ? true : voxel_.good()) &&
        (!voxel_score_.is_open() && IsPathEmpty(opt_.voxel_score_csv_path) ? true : voxel_score_.good()) &&
        (!edge_score_.is_open() && IsPathEmpty(opt_.edge_score_csv_path) ? true : edge_score_.good()) &&
        (!perf_.is_open() && IsPathEmpty(opt_.perf_csv_path) ? true : perf_.good());

  if (diag_.is_open()) SetStreamFormat(diag_, opt_.precision);
  if (topk_.is_open()) SetStreamFormat(topk_, opt_.precision);
  if (voxel_.is_open()) SetStreamFormat(voxel_, opt_.precision);
  if (voxel_score_.is_open()) SetStreamFormat(voxel_score_, opt_.precision);
  if (edge_score_.is_open()) SetStreamFormat(edge_score_, opt_.precision);
  if (perf_.is_open()) SetStreamFormat(perf_, opt_.precision);

  WriteHeadersIfNeeded();
}

CsvLogger::~CsvLogger() {
  if (diag_.is_open()) diag_.close();
  if (topk_.is_open()) topk_.close();
  if (voxel_.is_open()) voxel_.close();
  if (voxel_score_.is_open()) voxel_score_.close();
  if (edge_score_.is_open()) edge_score_.close();
  if (perf_.is_open()) perf_.close();
}

void CsvLogger::SetStreamFormat(std::ofstream& ofs, int precision) {
  ofs << std::fixed << std::setprecision(std::max(0, precision));
}

void CsvLogger::ResetT0(double t0_sec) {
  t0_ = t0_sec;
  has_t0_ = true;
}

double CsvLogger::RelTime(double current_time_sec) {
  if (!has_t0_) {
    t0_ = current_time_sec;
    has_t0_ = true;
  }
  return current_time_sec - t0_;
}

void CsvLogger::WriteHeadersIfNeeded() {
  if (diag_.is_open() && !diag_header_written_) {
    // EXACT header from your code
    diag_ << "t_rel,it,N,inlier_ratio,mean_res,median_res,mad_res,cond_JTJ,grad_norm,"
          << "dtrans,dyaw,droll,dpitch,yaw_raw,yaw_clamped,yaw_clamp_triggered,yaw_frozen,"
          << "eig_min,eig_max,eig_rank,num_corr,corr_p50,corr_p90,n_raw,n_split,"
          << "n_icp,n_map,icp_r0,icp_r1,icp_r2,map_r0,map_r1,map_r2,"
          << "alpha_p2plane,plane_used_ratio,plane_res_mean,"
          << "corr_highscore_ratio,highscore_fallback_ratio,score_tau_wall,score_filter_used,"
          << "w_mean,w_p90,w_min,w_max,w_highscore_ratio,score_mean_used,"
          << "corr_edge_ratio,corr_wall_ratio,corr_fallback_ratio,tau_edge,edge_first_used,"
          << "yaw_info,yaw_freeze_reason,cond_JTJ_scaled,"
          << "budget_edge,budget_wall,budget_fallback_eff,used_corr_total,"
          << "corr_edge_used,corr_wall_used,corr_fallback_used,corr_build_early_stop,"
          << "corr_queried_points,corr_budget_downgrade,ring_query_downgrade,"
          << "lm_first_enabled,lm_stage_used,lm_corr_used,refine_stage_mode,refine_corr_used,"
          << "lm_dx_norm,refine_dx_norm,lm_cost,refine_cost,lm_plane_used_ratio,"
          << "refine_plane_used_ratio,weighted_enabled,w_edge,w_wall,w_fallback,w_cap,"
          << "w_mean_used,w_min_used,w_max_used,w_edge_used_mean,w_wall_used_mean,"
          << "w_fallback_used_mean,cost_u,rel_drop_u,stop_by_cost_u\n";
    if (opt_.flush_each_write) diag_.flush();
    diag_header_written_ = true;
  }
  if (topk_.is_open() && !topk_header_written_) {
    // EXACT header from your code
    topk_ << "t_rel,it,idx,px,py,pz,qx,qy,qz,influence,weight\n";
    if (opt_.flush_each_write) topk_.flush();
    topk_header_written_ = true;
  }
  if (voxel_.is_open() && !voxel_header_written_) {
    voxel_ << "t_rel,voxel_count,mean_N,p90_N,eig_updated_voxels\n";
    if (opt_.flush_each_write) voxel_.flush();
    voxel_header_written_ = true;
  }
  if (voxel_score_.is_open() && !voxel_score_header_written_) {
    voxel_score_ << "t_rel,voxel_count,score_valid_ratio,score_mean,score_p90,score_p99,"
                 << "high_score_ratio,planarity_mean,linearity_mean,verticality_mean,temp_var_mean\n";
    if (opt_.flush_each_write) voxel_score_.flush();
    voxel_score_header_written_ = true;
  }
  if (edge_score_.is_open() && !edge_score_header_written_) {
    edge_score_ << "t_rel,voxel_count,edge_valid_ratio,edge_score_mean,edge_score_p90,"
                << "edge_score_p99,high_edge_ratio\n";
    if (opt_.flush_each_write) edge_score_.flush();
    edge_score_header_written_ = true;
  }
  if (perf_.is_open() && !perf_header_written_) {
    perf_ << "t_rel,dt_load,dt_preprocess,dt_icp_total,dt_corr_build,dt_query,dt_budget,"
          << "dt_accum,dt_solve,dt_map_update,dt_rebuild,dt_rebuild_async,dt_logging,used_corr,"
          << "corr_edge_ratio,corr_highscore_ratio,fallback_ratio,yaw_frozen,yaw_info,"
          << "waited_first_map_ms,skipped_icp_due_to_small_map,"
          << "query_edge_hits,query_wall_hits,query_fallback_hits,query_ring_used_mean,"
          << "corr_budget_downgrade,ring_query_downgrade,ring0_hit_ratio,ring_fallback_ratio,"
          << "adaptive_ring_enabled,adaptive_ring_degraded,adaptive_ring_hold_left,"
          << "effective_use_ring0,skipped_ring0_queries,"
          << "assoc_map_enabled,assoc_voxel,assoc_refine_with_fine,assoc_ring0_hit_ratio,"
          << "assoc_fallback_ratio,refine_used_ratio,"
          << "lm_first_enabled,lm_stage_used_ratio,lm_corr_used_mean,refine_corr_used_mean,"
          << "lm_dx_norm_mean,refine_dx_norm_mean,"
          << "use_corr_budget,corr_build_early_stop,budget_edge,budget_wall,budget_fallback_eff,"
          << "corr_edge_used,corr_wall_used,corr_fallback_used,used_corr_total,corr_queried_points,"
          << "weighted_enabled,w_mean_frame,w_max_frame,edge_ratio,wall_ratio,fallback_ratio_frame,"
          << "coarse_enabled,coarse_hit_ratio,coarse_refine_hit_ratio,coarse_fallback_ratio,"
          << "coarse_ring,refine_ring,fb_avg,coarse_hit_avg,"
          << "map_version,used_map_version\n";
    if (opt_.flush_each_write) perf_.flush();
    perf_header_written_ = true;
  }
}

void CsvLogger::LogIcpDiag(double current_time_sec,
                           int it,
                           std::size_t N,
                           double inlier_ratio,
                           double mean_res,
                           double median_res,
                           double mad_res,
                           double cond_JTJ,
                           double grad_norm) {
  if (!diag_.is_open()) return;

  const double t_rel = RelTime(current_time_sec);

  diag_ << t_rel << ","
        << it << ","
        << N << ","
        << inlier_ratio << ","
        << mean_res << ","
        << median_res << ","
        << mad_res << ","
        << cond_JTJ << ","
        << grad_norm << "\n";

  if (opt_.flush_each_write) diag_.flush();
}

void CsvLogger::LogTopK(double current_time_sec,
                        int it,
                        const std::vector<Eigen::Vector3d>& src_topK,
                        const std::vector<Eigen::Vector3d>& tgt_topK,
                        const std::vector<double>& influence_topK,
                        const std::vector<double>& weight_topK) {
  if (!topk_.is_open()) return;

  const std::size_t K = src_topK.size();
  if (tgt_topK.size() != K || influence_topK.size() != K || weight_topK.size() != K) {
    return;  // size mismatch -> skip (same behavior as your code)
  }

  const double t_rel = RelTime(current_time_sec);

  for (std::size_t k = 0; k < K; ++k) {
    const auto& p = src_topK[k];
    const auto& q = tgt_topK[k];
    topk_ << t_rel << ","
          << it << ","
          << k << ","
          << p.x() << "," << p.y() << "," << p.z() << ","
          << q.x() << "," << q.y() << "," << q.z() << ","
          << influence_topK[k] << ","
          << weight_topK[k] << "\n";
  }

  if (opt_.flush_each_write) topk_.flush();
}

}  // namespace struct_icp
