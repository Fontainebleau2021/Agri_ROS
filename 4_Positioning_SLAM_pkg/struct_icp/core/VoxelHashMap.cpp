#include "VoxelHashMap.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>

#include <Eigen/Eigenvalues>

namespace struct_icp {

void VoxelHashMap::Clear() {
  map_.clear();
  eig_updates_since_last_ = 0;
  time_counter_ = 0;
}

VoxelHashMap::VoxelKey VoxelHashMap::ToKey(const Eigen::Vector3d& p) const {
  if (p_.voxel_size <= 0.0) {
    return VoxelKey{};
  }
  const double inv = 1.0 / p_.voxel_size;
  VoxelKey k;
  k.x = static_cast<int>(std::floor(p.x() * inv));
  k.y = static_cast<int>(std::floor(p.y() * inv));
  k.z = static_cast<int>(std::floor(p.z() * inv));
  return k;
}

bool VoxelHashMap::SurfelVoxel::Add(const Eigen::Vector3d& p,
                                    double t,
                                    int eig_update_every_K,
                                    int eig_min_points,
                                    bool enable_surfel_stats,
                                    bool enable_score,
                                    double score_beta,
                                    double score_sigma_e,
                                    double* eig_ms_accum) {
  t_last = t;

  N += 1;
  if (!enable_surfel_stats) {
    const double a = 1.0 / static_cast<double>(N);
    mu = (1.0 - a) * mu + a * p;
    normal_valid = false;
    score_valid = false;
    return false;
  }

  const Eigen::Vector3d delta = p - mu;
  const double inv_n = 1.0 / static_cast<double>(N);
  mu += delta * inv_n;
  const Eigen::Vector3d delta2 = p - mu;
  M2.noalias() += delta * delta2.transpose();

  if (enable_score && normal_valid && N >= eig_min_points) {
    const double e = normal.dot(p - mu);
    const double beta = std::clamp(score_beta, 0.0, 1.0);
    m_e = (1.0 - beta) * m_e + beta * e;
    const double de = e - m_e;
    v_e = (1.0 - beta) * v_e + beta * (de * de);
    temp_var = v_e;
    if (score_sigma_e > 0.0) {
      temp_score = std::exp(-v_e / (score_sigma_e * score_sigma_e));
    }
  }

  if (eig_update_every_K <= 0) return false;
  if (N < eig_min_points) return false;
  if ((N % eig_update_every_K) != 0) return false;

  const double denom = std::max(1, N - 1);
  const Eigen::Matrix3d cov = M2 / static_cast<double>(denom);

  using Clock = std::chrono::steady_clock;
  const auto t_eig0 = (eig_ms_accum != nullptr) ? Clock::now() : Clock::time_point{};
  Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> es(cov);
  if (es.info() != Eigen::Success) return false;
  if (eig_ms_accum != nullptr) {
    const auto t_eig1 = Clock::now();
    *eig_ms_accum += std::chrono::duration<double, std::milli>(t_eig1 - t_eig0).count();
  }

  const Eigen::Vector3d evals = es.eigenvalues();
  const Eigen::Matrix3d evecs = es.eigenvectors();
  lambdas = evals;
  normal = evecs.col(0);
  line_dir = evecs.col(2);
  normal_valid = true;
  edge_valid = false;

  if (enable_score) {
    const double l1 = evals(2);
    const double l2 = evals(1);
    const double l3 = evals(0);
    const double denom_l = std::max(l1, 1e-12);
    planarity = (l2 - l3) / denom_l;
    linearity = (l1 - l2) / denom_l;
    verticality = 1.0 - std::abs(normal.dot(Eigen::Vector3d::UnitZ()));
    if (std::isfinite(planarity) && std::isfinite(linearity) &&
        std::isfinite(verticality)) {
      const double p_or_l = std::max(planarity, linearity);
      const double ts = std::isfinite(temp_score) ? temp_score : 0.0;
      const double raw = p_or_l * ts * verticality;
      score = std::clamp(raw, 0.0, 1.0);
      score_valid = true;

      const double vline = std::abs(line_dir.dot(Eigen::Vector3d::UnitZ()));
      const double raw_edge = linearity * vline * ts;
      edge_score = std::clamp(raw_edge, 0.0, 1.0);
      edge_valid = std::isfinite(edge_score);
    } else {
      score = 0.0;
      score_valid = false;
      edge_score = 0.0;
      edge_valid = false;
    }
  } else {
    score = 0.0;
    score_valid = false;
    edge_score = 0.0;
    edge_valid = false;
  }
  return true;
}

void VoxelHashMap::InsertPoints(const std::vector<Eigen::Vector3d>& pts_w) {
  using Clock = std::chrono::steady_clock;
  const auto t0 = perf_enabled_ ? Clock::now() : Clock::time_point{};
  map_.reserve(map_.size() + pts_w.size());
  for (const auto& pw : pts_w) {
    const auto k = ToKey(pw);
    const bool updated = map_[k].Add(
        pw,
        static_cast<double>(++time_counter_),
        p_.eig_update_every_K,
        p_.eig_min_points,
        p_.enable_surfel_stats,
        p_.enable_score,
        p_.score_beta,
        p_.score_sigma_e,
        perf_enabled_ ? &perf_.dt_eig_ms : nullptr);
    if (updated) ++eig_updates_since_last_;
  }
  if (perf_enabled_) {
    const auto t1 = Clock::now();
    perf_.dt_insert_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
  }
}

bool VoxelHashMap::QueryNearest(const Eigen::Vector3d& pw, Eigen::Vector3d& centroid_out) const {
  return QueryNearestWithRing(pw, p_.max_neighbor_ring, centroid_out);
}

bool VoxelHashMap::QueryNearestWithRing(const Eigen::Vector3d& pw,
                                        int ring,
                                        Eigen::Vector3d& centroid_out) const {
  return QueryNearestInRing(pw, ring, centroid_out);
}

bool VoxelHashMap::QueryNearestInRing(const Eigen::Vector3d& pw,
                                      int ring,
                                      Eigen::Vector3d& centroid_out) const {
  if (map_.empty()) return false;

  const auto base = ToKey(pw);
  const int r = (ring < 0) ? std::max(0, p_.max_neighbor_ring) : ring;

  double best_d2 = std::numeric_limits<double>::infinity();
  bool found = false;

  for (int dx = -r; dx <= r; ++dx) {
    for (int dy = -r; dy <= r; ++dy) {
      for (int dz = -r; dz <= r; ++dz) {
        VoxelKey k{base.x + dx, base.y + dy, base.z + dz};
        auto it = map_.find(k);
        if (it == map_.end()) continue;

        const Eigen::Vector3d& c = it->second.mu;
        const double d2 = (c - pw).squaredNorm();
        if (d2 < best_d2) {
          best_d2 = d2;
          centroid_out = c;
          found = true;
        }
      }
    }
  }
  return found;
}

bool VoxelHashMap::QueryNearestSurfel(const Eigen::Vector3d& pw, SurfelHit& out) const {
  return QueryNearestSurfelFilteredWithRing(
      pw, -std::numeric_limits<double>::infinity(), true, p_.max_neighbor_ring,
      out, nullptr, nullptr);
}

bool VoxelHashMap::QueryNearestSurfelWithRing(const Eigen::Vector3d& pw,
                                              int ring,
                                              SurfelHit& out) const {
  return QueryNearestSurfelFilteredWithRing(
      pw, -std::numeric_limits<double>::infinity(), true, ring,
      out, nullptr, nullptr);
}

bool VoxelHashMap::QueryNearestSurfelAroundKey(const VoxelKey& center_key,
                                               int ring,
                                               const Eigen::Vector3d& pw,
                                               SurfelHit& out) const {
  using Clock = std::chrono::steady_clock;
  const auto t0 = perf_enabled_ ? Clock::now() : Clock::time_point{};
  if (map_.empty()) return false;

  const int r = (ring < 0) ? std::max(0, p_.max_neighbor_ring) : ring;
  double best_d2 = std::numeric_limits<double>::infinity();
  bool found = false;

  auto update_hit = [&](const SurfelVoxel& v, const Eigen::Vector3d& c, double d2) {
    out.mu = c;
    out.normal = v.normal;
    out.normal_valid =
        p_.enable_surfel_stats &&
        v.normal_valid &&
        v.N >= p_.eig_min_points;
    if (out.normal_valid) {
      const double n2 = out.normal.squaredNorm();
      constexpr double kNormalEps = 1e-12;
      if (!std::isfinite(n2) || n2 <= kNormalEps) {
        out.normal_valid = false;
      } else {
        out.normal /= std::sqrt(n2);
      }
    }
    out.score = v.score;
    out.score_valid = v.score_valid;
    out.edge_score = v.edge_score;
    out.edge_valid = v.edge_valid && std::isfinite(v.edge_score);
    out.d2 = d2;
    out.passed_score_filter = false;
    out.used_fallback = false;
  };

  for (int dx = -r; dx <= r; ++dx) {
    for (int dy = -r; dy <= r; ++dy) {
      for (int dz = -r; dz <= r; ++dz) {
        VoxelKey k{center_key.x + dx, center_key.y + dy, center_key.z + dz};
        auto it = map_.find(k);
        if (it == map_.end()) continue;

        const auto& v = it->second;
        const Eigen::Vector3d& c = v.mu;
        const double d2 = (c - pw).squaredNorm();
        if (!found || d2 < best_d2) {
          best_d2 = d2;
          update_hit(v, c, d2);
          found = true;
        }
      }
    }
  }

  if (perf_enabled_) {
    const auto t1 = Clock::now();
    perf_.dt_query_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
  }
  return found;
}

bool VoxelHashMap::QueryNearestSurfelFiltered(const Eigen::Vector3d& pw,
                                              double tau_wall,
                                              bool allow_fallback,
                                              SurfelHit& out,
                                              bool* used_fallback,
                                              bool* passed_filter) const {
  return QueryNearestSurfelFilteredWithRing(
      pw, tau_wall, allow_fallback, p_.max_neighbor_ring, out, used_fallback, passed_filter);
}

bool VoxelHashMap::QueryNearestSurfelFilteredWithRing(const Eigen::Vector3d& pw,
                                                      double tau_wall,
                                                      bool allow_fallback,
                                                      int ring,
                                                      SurfelHit& out,
                                                      bool* used_fallback,
                                                      bool* passed_filter) const {
  using Clock = std::chrono::steady_clock;
  const auto t0 = perf_enabled_ ? Clock::now() : Clock::time_point{};
  if (map_.empty()) return false;

  const auto base = ToKey(pw);
  const int r = (ring < 0) ? std::max(0, p_.max_neighbor_ring) : ring;

  auto update_hit = [&](const SurfelVoxel& v, const Eigen::Vector3d& c, double d2) {
    out.mu = c;
    out.normal = v.normal;
    out.normal_valid =
        p_.enable_surfel_stats &&
        v.normal_valid &&
        v.N >= p_.eig_min_points;
    if (out.normal_valid) {
      const double n2 = out.normal.squaredNorm();
      constexpr double kNormalEps = 1e-12;
      if (!std::isfinite(n2) || n2 <= kNormalEps) {
        out.normal_valid = false;
      } else {
        out.normal /= std::sqrt(n2);
      }
    }
    out.score = v.score;
    out.score_valid = v.score_valid;
    out.edge_score = v.edge_score;
    out.edge_valid = v.edge_valid && std::isfinite(v.edge_score);
    out.d2 = d2;
  };

  double best_d2 = std::numeric_limits<double>::infinity();
  bool found = false;
  bool filter_found = false;
  SurfelHit filter_best;

  for (int dx = -r; dx <= r; ++dx) {
    for (int dy = -r; dy <= r; ++dy) {
      for (int dz = -r; dz <= r; ++dz) {
        VoxelKey k{base.x + dx, base.y + dy, base.z + dz};
        auto it = map_.find(k);
        if (it == map_.end()) continue;

        const auto& v = it->second;
        const Eigen::Vector3d& c = v.mu;
        const double d2 = (c - pw).squaredNorm();

        if (d2 < best_d2) {
          best_d2 = d2;
          update_hit(v, c, d2);
          found = true;
        }

        if (v.score_valid && v.score >= tau_wall) {
          if (!filter_found || d2 < filter_best.d2) {
            update_hit(v, c, d2);
            filter_best = out;
            filter_found = true;
          }
        }
      }
    }
  }

  if (passed_filter) *passed_filter = filter_found;
  if (filter_found) {
    if (used_fallback) *used_fallback = false;
    out = filter_best;
    out.passed_score_filter = true;
    out.used_fallback = false;
    if (perf_enabled_) {
      const auto t1 = Clock::now();
      perf_.dt_query_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
    }
    return true;
  }

  if (allow_fallback && found) {
    if (used_fallback) *used_fallback = true;
    out.passed_score_filter = false;
    out.used_fallback = true;
    if (perf_enabled_) {
      const auto t1 = Clock::now();
      perf_.dt_query_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
    }
    return true;
  }

  if (used_fallback) *used_fallback = false;
  if (perf_enabled_) {
    const auto t1 = Clock::now();
    perf_.dt_query_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
  }
  return false;
}

bool VoxelHashMap::QueryNearestHierarchical(const Eigen::Vector3d& pw,
                                            double tau_edge,
                                            double tau_wall,
                                            bool allow_fallback,
                                            SurfelHit& out,
                                            MatchTier* tier_out) const {
  return QueryNearestHierarchicalWithRings(
      pw, tau_edge, tau_wall, allow_fallback,
      p_.max_neighbor_ring, p_.max_neighbor_ring, p_.max_neighbor_ring,
      out, tier_out, nullptr);
}

bool VoxelHashMap::QueryNearestHierarchicalWithRings(const Eigen::Vector3d& pw,
                                                     double tau_edge,
                                                     double tau_wall,
                                                     bool allow_fallback,
                                                     int ring_edge,
                                                     int ring_wall,
                                                     int ring_fallback,
                                                     SurfelHit& out,
                                                     MatchTier* tier_out,
                                                     int* ring_used) const {
  using Clock = std::chrono::steady_clock;
  const auto t0 = perf_enabled_ ? Clock::now() : Clock::time_point{};
  if (map_.empty()) return false;

  const auto base = ToKey(pw);
  const int r_edge = (ring_edge < 0) ? std::max(0, p_.max_neighbor_ring) : ring_edge;
  const int r_wall = (ring_wall < 0) ? std::max(0, p_.max_neighbor_ring) : ring_wall;
  const int r_fb = (ring_fallback < 0) ? std::max(0, p_.max_neighbor_ring) : ring_fallback;

  auto update_hit = [&](const SurfelVoxel& v, const Eigen::Vector3d& c, double d2) {
    out.mu = c;
    out.normal = v.normal;
    out.normal_valid =
        p_.enable_surfel_stats &&
        v.normal_valid &&
        v.N >= p_.eig_min_points;
    if (out.normal_valid) {
      const double n2 = out.normal.squaredNorm();
      constexpr double kNormalEps = 1e-12;
      if (!std::isfinite(n2) || n2 <= kNormalEps) {
        out.normal_valid = false;
      } else {
        out.normal /= std::sqrt(n2);
      }
    }
    out.score = v.score;
    out.score_valid = v.score_valid;
    out.edge_score = v.edge_score;
    out.edge_valid = v.edge_valid && std::isfinite(v.edge_score);
    out.d2 = d2;
  };

  auto find_best = [&](int r,
                       const std::function<bool(const SurfelVoxel&)>& pred,
                       SurfelHit* best_out) -> bool {
    bool found = false;
    double best_d2 = std::numeric_limits<double>::infinity();
    for (int dx = -r; dx <= r; ++dx) {
      for (int dy = -r; dy <= r; ++dy) {
        for (int dz = -r; dz <= r; ++dz) {
          VoxelKey k{base.x + dx, base.y + dy, base.z + dz};
          auto it = map_.find(k);
          if (it == map_.end()) continue;

          const auto& v = it->second;
          if (!pred(v)) continue;
          const Eigen::Vector3d& c = v.mu;
          const double d2 = (c - pw).squaredNorm();
          if (!found || d2 < best_d2) {
            best_d2 = d2;
            update_hit(v, c, d2);
            *best_out = out;
            found = true;
          }
        }
      }
    }
    return found;
  };

  SurfelHit edge_best;
  if (find_best(r_edge,
                [&](const SurfelVoxel& v) {
                  return v.edge_valid && v.edge_score >= tau_edge;
                },
                &edge_best)) {
    out = edge_best;
    out.passed_score_filter = false;
    out.used_fallback = false;
    if (tier_out) *tier_out = MatchTier::EDGE;
    if (ring_used) *ring_used = r_edge;
    if (perf_enabled_) {
      const auto t1 = Clock::now();
      perf_.dt_query_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
    }
    return true;
  }

  SurfelHit wall_best;
  if (find_best(r_wall,
                [&](const SurfelVoxel& v) {
                  return v.score_valid && v.score >= tau_wall;
                },
                &wall_best)) {
    out = wall_best;
    out.passed_score_filter = true;
    out.used_fallback = false;
    if (tier_out) *tier_out = MatchTier::WALL;
    if (ring_used) *ring_used = r_wall;
    if (perf_enabled_) {
      const auto t1 = Clock::now();
      perf_.dt_query_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
    }
    return true;
  }

  if (allow_fallback) {
    SurfelHit fb_best;
    if (find_best(r_fb,
                  [&](const SurfelVoxel&) { return true; },
                  &fb_best)) {
      out = fb_best;
      out.passed_score_filter = false;
      out.used_fallback = true;
      if (tier_out) *tier_out = MatchTier::FALLBACK;
      if (ring_used) *ring_used = r_fb;
      if (perf_enabled_) {
        const auto t1 = Clock::now();
        perf_.dt_query_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
      }
      return true;
    }
  }

  if (perf_enabled_) {
    const auto t1 = Clock::now();
    perf_.dt_query_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
  }
  return false;
}

VoxelHashMap::StatsSummary VoxelHashMap::GetStatsSummaryAndReset() {
  StatsSummary st;
  st.voxel_count = map_.size();
  if (map_.empty()) {
    st.eig_updated_voxels = eig_updates_since_last_;
    eig_updates_since_last_ = 0;
    return st;
  }

  std::vector<int> counts;
  counts.reserve(map_.size());
  std::size_t sum_N = 0;
  for (const auto& kv : map_) {
    const int n = kv.second.N;
    counts.push_back(n);
    sum_N += static_cast<std::size_t>(n);
  }

  st.mean_N = static_cast<double>(sum_N) / static_cast<double>(counts.size());
  const std::size_t idx = static_cast<std::size_t>(
      std::max<std::size_t>(1, (counts.size() * 9 + 9) / 10)) - 1;
  std::nth_element(counts.begin(), counts.begin() + idx, counts.end());
  st.p90_N = counts[idx];
  st.eig_updated_voxels = eig_updates_since_last_;
  eig_updates_since_last_ = 0;
  return st;
}

VoxelHashMap::ScoreSummary VoxelHashMap::GetScoreSummary() const {
  ScoreSummary st;
  st.voxel_count = map_.size();
  if (map_.empty()) return st;

  std::vector<double> scores;
  scores.reserve(map_.size());
  std::size_t valid = 0;
  std::size_t high = 0;
  double sum_score = 0.0;
  double sum_planarity = 0.0;
  double sum_linearity = 0.0;
  double sum_verticality = 0.0;
  double sum_temp_var = 0.0;

  for (const auto& kv : map_) {
    const auto& v = kv.second;
    if (!v.score_valid) continue;
    valid += 1;
    scores.push_back(v.score);
    sum_score += v.score;
    sum_planarity += v.planarity;
    sum_linearity += v.linearity;
    sum_verticality += v.verticality;
    sum_temp_var += v.temp_var;
    if (v.score >= p_.score_tau_wall) high += 1;
  }

  if (valid == 0) return st;
  st.score_valid_ratio = static_cast<double>(valid) / static_cast<double>(st.voxel_count);
  st.score_mean = sum_score / static_cast<double>(valid);
  st.planarity_mean = sum_planarity / static_cast<double>(valid);
  st.linearity_mean = sum_linearity / static_cast<double>(valid);
  st.verticality_mean = sum_verticality / static_cast<double>(valid);
  st.temp_var_mean = sum_temp_var / static_cast<double>(valid);
  st.high_score_ratio = static_cast<double>(high) / static_cast<double>(valid);

  std::sort(scores.begin(), scores.end());
  const std::size_t idx90 = std::min(scores.size() - 1, (scores.size() * 9) / 10);
  const std::size_t idx99 = std::min(scores.size() - 1, (scores.size() * 99) / 100);
  st.score_p90 = scores[idx90];
  st.score_p99 = scores[idx99];
  return st;
}

VoxelHashMap::EdgeScoreSummary VoxelHashMap::GetEdgeScoreSummary(double tau_edge) const {
  EdgeScoreSummary st;
  st.voxel_count = map_.size();
  if (map_.empty()) return st;

  std::vector<double> scores;
  scores.reserve(map_.size());
  std::size_t valid = 0;
  std::size_t high = 0;
  double sum_score = 0.0;

  for (const auto& kv : map_) {
    const auto& v = kv.second;
    if (!v.edge_valid) continue;
    valid += 1;
    scores.push_back(v.edge_score);
    sum_score += v.edge_score;
    if (v.edge_score >= tau_edge) high += 1;
  }

  if (valid == 0) return st;
  st.edge_valid_ratio = static_cast<double>(valid) / static_cast<double>(st.voxel_count);
  st.edge_score_mean = sum_score / static_cast<double>(valid);
  st.high_edge_ratio = static_cast<double>(high) / static_cast<double>(valid);

  const std::size_t idx90 = std::min(scores.size() - 1, (scores.size() * 9) / 10);
  const std::size_t idx99 = std::min(scores.size() - 1, (scores.size() * 99) / 100);
  std::nth_element(scores.begin(), scores.begin() + idx90, scores.end());
  st.edge_score_p90 = scores[idx90];
  std::nth_element(scores.begin(), scores.begin() + idx99, scores.end());
  st.edge_score_p99 = scores[idx99];
  return st;
}

void VoxelHashMap::ResetPerf() {
  perf_ = PerfCounters{};
}

VoxelHashMap::PerfCounters VoxelHashMap::GetPerfAndReset() {
  PerfCounters out = perf_;
  perf_ = PerfCounters{};
  return out;
}

}  // namespace struct_icp
