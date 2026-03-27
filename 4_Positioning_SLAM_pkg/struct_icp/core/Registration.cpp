#include "Registration.hpp"

#include <Eigen/Cholesky>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>

#include "Statistics.hpp"

namespace struct_icp {

namespace {
inline double WrapPi(double a) {
  while (a > M_PI) a -= 2.0*M_PI;
  while (a < -M_PI) a += 2.0*M_PI;
  return a;
}

inline void RpyFromR(const Eigen::Matrix3d& R, double& roll, double& pitch, double& yaw) {
  // ZYX (yaw-pitch-roll), right-handed
  yaw   = std::atan2(R(1,0), R(0,0));
  pitch = std::asin(std::max(-1.0, std::min(1.0, -R(2,0))));
  roll  = std::atan2(R(2,1), R(2,2));
}
} // namespace


static inline Eigen::Matrix3d Hat(const Eigen::Vector3d& v) {
  Eigen::Matrix3d m;
  m <<     0.0, -v.z(),  v.y(),
        v.z(),    0.0, -v.x(),
       -v.y(),  v.x(),   0.0;
  return m;
}


namespace {
struct Corr {
  Eigen::Vector3d pw;
  Eigen::Vector3d q;
  Eigen::Vector3d normal;
  bool normal_valid{false};
  double d2{0.0};
  double w{1.0};
  double score{0.0};
  bool score_valid{false};
  double edge_score{0.0};
  bool edge_valid{false};
  bool passed_score_filter{false};
  bool used_fallback{false};
};
}  // namespace

void Registration::SetMapFromPointsWorld(
    const std::vector<Eigen::Vector3d>& pts_w) {
  map_.Clear();
  map_.InsertPoints(pts_w);
}

bool Registration::AlignPointToVoxelMap(
    const std::vector<PointXYZI>& scan_body,
    Sophus::SE3d& T_wb_io,
    std::vector<IcpIterStats>* iter_stats,
    std::vector<IcpTopKStats>* topk_stats,
    PerfStats* perf) const {
  // 旧接口：默认对齐到内部 map_
  return AlignPointToVoxelMap(map_, scan_body, T_wb_io, iter_stats, topk_stats, perf);
}

bool Registration::AlignPointToVoxelMap(
    const VoxelHashMap& map,
    const std::vector<PointXYZI>& scan_body,
    Sophus::SE3d& T_wb_io,
    std::vector<IcpIterStats>* iter_stats,
    std::vector<IcpTopKStats>* topk_stats,
    PerfStats* perf,
    const AlignOptions* opts) const {

  if (scan_body.empty()) return false;

  if (iter_stats) iter_stats->clear();
  if (topk_stats) topk_stats->clear();
  if (perf) *perf = PerfStats{};
  using Clock = std::chrono::steady_clock;

  Sophus::SE3d T = T_wb_io;
  const double max_corr2 = rp_.max_corr_dist * rp_.max_corr_dist;
  const double alpha = std::clamp(rp_.alpha_point_to_plane, 0.0, 1.0);
  const double w_pt = 1.0 - alpha;
  const double w_base = rp_.w_base;
  const double w_gain = rp_.w_score_gain;
  const double w_min = rp_.w_min;
  const double w_max = rp_.w_max;
  const int max_corr = rp_.max_corr_per_iter;
  const int ring_default = map.max_neighbor_ring();
  const int ring_edge = rp_.use_ring_query ? rp_.ring_edge : ring_default;
  const int ring_wall = rp_.use_ring_query ? rp_.ring_wall : ring_default;
  int ring_fallback = rp_.use_ring_query ? rp_.ring_fallback : ring_default;
  const bool use_ring_query_effective =
      opts ? opts->use_ring_query_effective : rp_.use_ring_query;
  const bool skip_ring0 = rp_.use_ring_query && !use_ring_query_effective;
  if (opts && opts->has_ring_fallback_override) {
    ring_fallback = opts->ring_fallback_override;
  }
  const bool refine_with_fine =
      opts && opts->refine_with_fine && opts->refine_map;
  const int refine_ring = opts ? opts->refine_ring : 1;
  const VoxelHashMap* refine_map = opts ? opts->refine_map : nullptr;
  const bool enable_coarse_assoc =
      opts && opts->enable_coarse_assoc && opts->fine_map;
  const int coarse_ring = opts ? opts->coarse_ring : 0;
  const int coarse_refine_ring = opts ? opts->coarse_refine_ring : 0;
  const bool coarse_fallback_full = opts ? opts->coarse_fallback_full : true;
  const VoxelHashMap* fine_map = opts ? opts->fine_map : nullptr;

  int budget_edge = std::numeric_limits<int>::max();
  int budget_wall = std::numeric_limits<int>::max();
  int budget_fallback = std::numeric_limits<int>::max();
  if (rp_.use_corr_budget && max_corr > 0) {
    double re = std::max(0.0, rp_.corr_budget_edge_ratio);
    double rw = std::max(0.0, rp_.corr_budget_wall_ratio);
    double rf = std::max(0.0, rp_.corr_budget_fallback_ratio);
    double sum = re + rw + rf;
    if (sum <= 0.0) {
      re = 0.0; rw = 0.0; rf = 1.0; sum = 1.0;
    }
    budget_edge = static_cast<int>(std::lround(max_corr * (re / sum)));
    budget_wall = static_cast<int>(std::lround(max_corr * (rw / sum)));
    budget_fallback = max_corr - budget_edge - budget_wall;
    if (budget_fallback < 1) {
      const int need = 1 - budget_fallback;
      if (budget_wall >= need) {
        budget_wall -= need;
      } else {
        const int rem = need - budget_wall;
        budget_wall = 0;
        budget_edge = std::max(0, budget_edge - rem);
      }
      budget_fallback = 1;
    }
    if (budget_fallback < 0) budget_fallback = 0;
  }

  std::vector<double> residuals;
  residuals.reserve(scan_body.size());

  // For Top-K (on USED correspondences)
  std::vector<Eigen::Vector3d> src_inliers;
  std::vector<Eigen::Vector3d> tgt_inliers;
  std::vector<double> influence_key;
  std::vector<double> weight_val;

  // NEW: buffer correspondences each iteration
  std::vector<Corr> corrs;
  corrs.reserve(scan_body.size());

  double cost_prev = std::numeric_limits<double>::infinity();
  double cost_u_prev = std::numeric_limits<double>::infinity();
  double last_eig_min = 0.0;
  double last_eig_max = 0.0;
  int last_eig_rank = 0;

  for (int iter = 0; iter < rp_.max_iters; ++iter) {
    residuals.clear();
    src_inliers.clear();
    tgt_inliers.clear();
    influence_key.clear();
    weight_val.clear();
    corrs.clear();

    // 1) build candidate correspondences
    const auto t_corr0 = perf ? Clock::now() : Clock::time_point{};
    std::size_t highscore_used = 0;
    std::size_t fallback_used = 0;
    std::size_t attempts = 0;
    std::size_t edge_used = 0;
    std::size_t wall_used = 0;
    std::size_t weight_boost_used = 0;
    std::size_t query_edge_hits = 0;
    std::size_t query_wall_hits = 0;
    std::size_t query_fallback_hits = 0;
    std::size_t query_ring0_hits = 0;
    std::size_t skipped_ring0_queries = 0;
    std::size_t refine_used = 0;
    std::size_t coarse_hits = 0;
    std::size_t coarse_refine_hits = 0;
    std::size_t coarse_fallback_hits = 0;
    double ring_used_sum = 0.0;
    std::size_t ring_used_cnt = 0;
    int corr_build_early_stop = 0;
    int corr_budget_downgrade = 0;
    int ring_query_downgrade = 0;
    std::vector<double> weights;
    weights.reserve(scan_body.size());
    std::vector<double> used_scores;
    used_scores.reserve(scan_body.size());

    if (rp_.use_corr_budget) {
      std::vector<Corr> corrs_edge;
      std::vector<Corr> corrs_wall;
      std::vector<Corr> corrs_fallback;
      corrs_edge.reserve(scan_body.size());
      corrs_wall.reserve(scan_body.size());
      corrs_fallback.reserve(scan_body.size());

      int budget_fallback_eff = budget_fallback;
      int used_total = 0;
      for (const auto& pt : scan_body) {
        budget_fallback_eff = max_corr - static_cast<int>(corrs_edge.size())
                            - static_cast<int>(corrs_wall.size());
        if (budget_fallback_eff < 0) budget_fallback_eff = 0;
        if (rp_.corr_build_early_stop && max_corr > 0 && used_total >= max_corr) {
          corr_build_early_stop = 1;
          break;
        }
        attempts += 1;
        const Eigen::Vector3d pw = T * pt.p;

        VoxelHashMap::SurfelHit hit;
        bool used_fallback = false;
        bool passed_filter = false;
        VoxelHashMap::MatchTier tier = VoxelHashMap::MatchTier::FALLBACK;
        int ring_used = -1;
        bool ok = false;

        if (enable_coarse_assoc) {
          VoxelHashMap::SurfelHit hit_c;
          bool coarse_ok = false;
          bool coarse_passed_filter = false;
          VoxelHashMap::MatchTier coarse_tier = VoxelHashMap::MatchTier::FALLBACK;
          if (rp_.use_edge_first) {
            coarse_ok = map.QueryNearestHierarchicalWithRings(pw,
                                                              rp_.tau_edge,
                                                              rp_.score_tau_wall,
                                                              rp_.score_allow_fallback,
                                                              coarse_ring,
                                                              coarse_ring,
                                                              coarse_ring,
                                                              hit_c,
                                                              &coarse_tier,
                                                              &ring_used);
          } else if (rp_.use_score_filter) {
            coarse_ok = map.QueryNearestSurfelFilteredWithRing(pw,
                                                               rp_.score_tau_wall,
                                                               rp_.score_allow_fallback,
                                                               coarse_ring,
                                                               hit_c,
                                                               nullptr,
                                                               &coarse_passed_filter);
            ring_used = coarse_ring;
          } else {
            coarse_ok = map.QueryNearestSurfelWithRing(pw, coarse_ring, hit_c);
            ring_used = coarse_ring;
          }

          if (coarse_ok) {
            coarse_hits += 1;
            const auto key = fine_map->KeyFromPoint(hit_c.mu);
            VoxelHashMap::SurfelHit hit_f;
            bool refine_ok =
                fine_map->QueryNearestSurfelAroundKey(
                    key, coarse_refine_ring, pw, hit_f);
            if (refine_ok) {
              coarse_refine_hits += 1;
              hit = hit_f;
              if (rp_.use_edge_first) {
                const bool edge_ok = hit.edge_valid && hit.edge_score >= rp_.tau_edge;
                const bool wall_ok = hit.score_valid && hit.score >= rp_.score_tau_wall;
                if (coarse_tier == VoxelHashMap::MatchTier::EDGE && edge_ok) {
                  tier = VoxelHashMap::MatchTier::EDGE;
                  hit.passed_score_filter = false;
                  hit.used_fallback = false;
                  ok = true;
                } else if (coarse_tier == VoxelHashMap::MatchTier::WALL && wall_ok) {
                  tier = VoxelHashMap::MatchTier::WALL;
                  hit.passed_score_filter = true;
                  hit.used_fallback = false;
                  ok = true;
                } else if (coarse_tier == VoxelHashMap::MatchTier::FALLBACK) {
                  tier = VoxelHashMap::MatchTier::FALLBACK;
                  hit.passed_score_filter = false;
                  hit.used_fallback = true;
                  ok = true;
                }
              } else if (rp_.use_score_filter) {
                const bool wall_ok = hit.score_valid && hit.score >= rp_.score_tau_wall;
                if (coarse_passed_filter) {
                  if (wall_ok) {
                    ok = true;
                    used_fallback = false;
                    passed_filter = true;
                    hit.passed_score_filter = true;
                    hit.used_fallback = false;
                  }
                } else if (rp_.score_allow_fallback) {
                  ok = true;
                  used_fallback = !wall_ok;
                  passed_filter = wall_ok;
                  hit.passed_score_filter = wall_ok;
                  hit.used_fallback = !wall_ok;
                } else if (wall_ok) {
                  ok = true;
                  used_fallback = false;
                  passed_filter = true;
                  hit.passed_score_filter = true;
                  hit.used_fallback = false;
                }
              } else {
                hit.passed_score_filter = hit.score_valid && hit.score >= rp_.score_tau_wall;
                hit.used_fallback = false;
                tier = VoxelHashMap::MatchTier::FALLBACK;
                ok = true;
              }
            }
          }

          if (!ok && coarse_fallback_full && fine_map) {
            coarse_fallback_hits += 1;
            if (rp_.use_edge_first) {
              ok = fine_map->QueryNearestHierarchical(pw,
                                                      rp_.tau_edge,
                                                      rp_.score_tau_wall,
                                                      rp_.score_allow_fallback,
                                                      hit,
                                                      &tier);
              ring_used = ring_default;
            } else if (rp_.use_score_filter) {
              ok = fine_map->QueryNearestSurfelFiltered(pw,
                                                        rp_.score_tau_wall,
                                                        rp_.score_allow_fallback,
                                                        hit,
                                                        &used_fallback,
                                                        &passed_filter);
              ring_used = ring_default;
              if (ok) {
                tier = used_fallback ? VoxelHashMap::MatchTier::FALLBACK
                                     : VoxelHashMap::MatchTier::WALL;
              }
            } else {
              ok = fine_map->QueryNearestSurfel(pw, hit);
              ring_used = ring_default;
              if (ok) {
                hit.passed_score_filter = hit.score_valid && hit.score >= rp_.score_tau_wall;
                hit.used_fallback = false;
                tier = VoxelHashMap::MatchTier::FALLBACK;
              }
            }
          }
        } else if (skip_ring0) {
          skipped_ring0_queries += 1;
          ok = map.QueryNearestSurfelWithRing(pw, ring_fallback, hit);
          if (ok) {
            used_fallback = true;
            hit.used_fallback = true;
            hit.passed_score_filter = false;
            tier = VoxelHashMap::MatchTier::FALLBACK;
            ring_used = ring_fallback;
            query_fallback_hits += 1;
          }
        } else if (rp_.use_edge_first) {
          ok = map.QueryNearestHierarchicalWithRings(pw,
                                                     rp_.tau_edge,
                                                     rp_.score_tau_wall,
                                                     rp_.score_allow_fallback,
                                                     ring_edge,
                                                     ring_wall,
                                                     ring_fallback,
                                                     hit,
                                                     &tier,
                                                     &ring_used);
        } else if (rp_.use_score_filter) {
          ok = map.QueryNearestSurfelFilteredWithRing(pw,
                                                      rp_.score_tau_wall,
                                                      false,
                                                      ring_wall,
                                                      hit,
                                                      &used_fallback,
                                                      &passed_filter);
          if (!ok && rp_.score_allow_fallback) {
            ok = map.QueryNearestSurfelWithRing(pw, ring_fallback, hit);
            if (ok) {
              used_fallback = true;
              passed_filter = false;
              hit.used_fallback = true;
              hit.passed_score_filter = false;
            }
          }
          if (ok) {
            tier = used_fallback ? VoxelHashMap::MatchTier::FALLBACK
                                 : VoxelHashMap::MatchTier::WALL;
            ring_used = used_fallback ? ring_fallback : ring_wall;
          }
        } else {
          ok = map.QueryNearestSurfelWithRing(pw, ring_fallback, hit);
          if (ok) {
            hit.passed_score_filter = hit.score_valid && hit.score >= rp_.score_tau_wall;
            hit.used_fallback = false;
            tier = VoxelHashMap::MatchTier::FALLBACK;
            ring_used = ring_fallback;
          }
        }
        if (!ok) continue;
        if (refine_with_fine && refine_map) {
          VoxelHashMap::SurfelHit fine_hit;
          if (refine_map->QueryNearestSurfelWithRing(pw, refine_ring, fine_hit)) {
            hit.mu = fine_hit.mu;
            hit.normal = fine_hit.normal;
            hit.normal_valid = fine_hit.normal_valid;
            refine_used += 1;
          }
        }
        if (ring_used >= 0) {
          ring_used_sum += static_cast<double>(ring_used);
          ring_used_cnt += 1;
          if (ring_used == 0) query_ring0_hits += 1;
        }

        const Eigen::Vector3d r = pw - hit.mu;
        const double d2 = r.squaredNorm();
        if (d2 > max_corr2) continue;

        double w = 1.0;
        bool boosted = false;
        if (rp_.use_score_weight) {
          const bool use_score = hit.score_valid &&
              (!rp_.weight_only_highscore || hit.passed_score_filter) &&
              !hit.used_fallback;
          if (use_score) {
            const double w_raw = w_base + w_gain * hit.score;
            w = std::clamp(w_raw, w_min, w_max);
            boosted = true;
          } else {
            w = w_base;
          }
        }

        const bool is_edge = (tier == VoxelHashMap::MatchTier::EDGE);
        const bool is_wall = (!is_edge) && hit.score_valid && (hit.score >= rp_.score_tau_wall);
        const Corr c{
            pw, hit.mu, hit.normal, hit.normal_valid, d2, w, hit.score,
            hit.score_valid, hit.edge_score, hit.edge_valid,
            hit.passed_score_filter, hit.used_fallback};
        if (is_edge) {
          corrs_edge.push_back(c);
          query_edge_hits += 1;
        } else if (is_wall) {
          corrs_wall.push_back(c);
          query_wall_hits += 1;
        } else {
          corrs_fallback.push_back(c);
          query_fallback_hits += 1;
        }
        used_total = static_cast<int>(corrs_edge.size() + corrs_wall.size() + corrs_fallback.size());

        weights.push_back(w);
        if (boosted) weight_boost_used += 1;
        if (hit.score_valid) used_scores.push_back(hit.score);
      }

      const auto t_budget0 = perf ? Clock::now() : Clock::time_point{};
      if (max_corr > 0) {
        budget_fallback_eff = max_corr - static_cast<int>(corrs_edge.size())
                            - static_cast<int>(corrs_wall.size());
        if (budget_fallback_eff < 0) budget_fallback_eff = 0;
        if (budget_edge > 0 && static_cast<int>(corrs_edge.size()) > budget_edge) {
          auto mid = corrs_edge.begin() + budget_edge;
          std::nth_element(corrs_edge.begin(), mid, corrs_edge.end(),
                           [](const Corr& a, const Corr& b) { return a.d2 < b.d2; });
          corrs_edge.resize(budget_edge);
        }
        if (budget_wall > 0 && static_cast<int>(corrs_wall.size()) > budget_wall) {
          auto mid = corrs_wall.begin() + budget_wall;
          std::nth_element(corrs_wall.begin(), mid, corrs_wall.end(),
                           [](const Corr& a, const Corr& b) { return a.d2 < b.d2; });
          corrs_wall.resize(budget_wall);
        }
        budget_fallback_eff = max_corr - static_cast<int>(corrs_edge.size())
                            - static_cast<int>(corrs_wall.size());
        if (budget_fallback_eff < 0) budget_fallback_eff = 0;
        if (budget_fallback_eff > 0 &&
            static_cast<int>(corrs_fallback.size()) > budget_fallback_eff) {
          auto mid = corrs_fallback.begin() + budget_fallback_eff;
          std::nth_element(corrs_fallback.begin(), mid, corrs_fallback.end(),
                           [](const Corr& a, const Corr& b) { return a.d2 < b.d2; });
          corrs_fallback.resize(budget_fallback_eff);
        }
      }
      corrs.insert(corrs.end(), corrs_edge.begin(), corrs_edge.end());
      corrs.insert(corrs.end(), corrs_wall.begin(), corrs_wall.end());
      corrs.insert(corrs.end(), corrs_fallback.begin(), corrs_fallback.end());

      edge_used = corrs_edge.size();
      wall_used = corrs_wall.size();
      fallback_used = corrs_fallback.size();
      highscore_used = wall_used;

      weights.clear();
      used_scores.clear();
      weight_boost_used = 0;
      for (const auto& c : corrs) {
        weights.push_back(c.w);
        if (c.score_valid) used_scores.push_back(c.score);
        if (c.w > w_base + 1e-12) weight_boost_used += 1;
      }

      if (perf) {
        const auto t_budget1 = Clock::now();
        perf->dt_budget_ms += std::chrono::duration<double, std::milli>(t_budget1 - t_budget0).count();
      }

      if (static_cast<int>(corrs.size()) < rp_.min_effective_corr) {
        corr_budget_downgrade = 1;
        ring_query_downgrade = rp_.use_ring_query ? 1 : 0;

        corrs.clear();
        weights.clear();
        used_scores.clear();
        edge_used = 0;
        wall_used = 0;
        fallback_used = 0;
        highscore_used = 0;
        attempts = 0;
        query_edge_hits = 0;
        query_wall_hits = 0;
        query_fallback_hits = 0;
        query_ring0_hits = 0;
        skipped_ring0_queries = 0;
        refine_used = 0;
        coarse_hits = 0;
        coarse_refine_hits = 0;
        coarse_fallback_hits = 0;
        ring_used_sum = 0.0;
        ring_used_cnt = 0;
        corr_build_early_stop = 0;

        for (const auto& pt : scan_body) {
          attempts += 1;
          const Eigen::Vector3d pw = T * pt.p;

          VoxelHashMap::SurfelHit hit;
          bool used_fallback = false;
          bool passed_filter = false;
          const bool ok = rp_.use_edge_first
              ? map.QueryNearestHierarchical(pw,
                                             rp_.tau_edge,
                                             rp_.score_tau_wall,
                                             rp_.score_allow_fallback,
                                             hit,
                                             nullptr)
              : (rp_.use_score_filter
                     ? map.QueryNearestSurfelFiltered(pw,
                                                      rp_.score_tau_wall,
                                                      rp_.score_allow_fallback,
                                                      hit,
                                                      &used_fallback,
                                                      &passed_filter)
                     : map.QueryNearestSurfel(pw, hit));
          if (!ok) continue;
          if (refine_with_fine && refine_map) {
            VoxelHashMap::SurfelHit fine_hit;
            if (refine_map->QueryNearestSurfelWithRing(pw, refine_ring, fine_hit)) {
              hit.mu = fine_hit.mu;
              hit.normal = fine_hit.normal;
              hit.normal_valid = fine_hit.normal_valid;
              refine_used += 1;
            }
          }
          if (!rp_.use_score_filter && !rp_.use_edge_first) {
            hit.passed_score_filter = hit.score_valid && hit.score >= rp_.score_tau_wall;
            hit.used_fallback = false;
          }
          if (rp_.use_edge_first) {
            if (hit.used_fallback) {
              fallback_used += 1;
            } else if (hit.passed_score_filter) {
              wall_used += 1;
            } else {
              edge_used += 1;
            }
          } else {
            if (rp_.use_score_filter && passed_filter && !used_fallback) highscore_used += 1;
            if (rp_.use_score_filter && used_fallback) fallback_used += 1;
          }

          const Eigen::Vector3d r = pw - hit.mu;
          const double d2 = r.squaredNorm();
          if (d2 > max_corr2) continue;

          double w = 1.0;
          bool boosted = false;
          if (rp_.use_score_weight) {
            const bool use_score = hit.score_valid &&
                (!rp_.weight_only_highscore || hit.passed_score_filter) &&
                !hit.used_fallback;
            if (use_score) {
              const double w_raw = w_base + w_gain * hit.score;
              w = std::clamp(w_raw, w_min, w_max);
              boosted = true;
            } else {
              w = w_base;
            }
          }
          corrs.push_back(Corr{
              pw, hit.mu, hit.normal, hit.normal_valid, d2, w, hit.score,
              hit.score_valid, hit.edge_score, hit.edge_valid,
              hit.passed_score_filter, hit.used_fallback});
          weights.push_back(w);
          if (boosted) weight_boost_used += 1;
          if (hit.score_valid) used_scores.push_back(hit.score);
        }

        if (max_corr > 0 && static_cast<int>(corrs.size()) > max_corr) {
          auto mid = corrs.begin() + max_corr;
          std::nth_element(
              corrs.begin(), mid, corrs.end(),
              [](const Corr& a, const Corr& b) { return a.d2 < b.d2; });
          corrs.resize(max_corr);
        }
        query_edge_hits = edge_used;
        query_wall_hits = wall_used;
        query_fallback_hits = fallback_used;
      }
    } else {
      for (const auto& pt : scan_body) {
        attempts += 1;
        const Eigen::Vector3d pw = T * pt.p;

        VoxelHashMap::SurfelHit hit;
        bool used_fallback = false;
        bool passed_filter = false;
        bool ok = false;
        if (enable_coarse_assoc) {
          VoxelHashMap::SurfelHit hit_c;
          bool coarse_ok = false;
          bool coarse_passed_filter = false;
          VoxelHashMap::MatchTier coarse_tier = VoxelHashMap::MatchTier::FALLBACK;
          if (rp_.use_edge_first) {
            ok = map.QueryNearestHierarchicalWithRings(pw,
                                                       rp_.tau_edge,
                                                       rp_.score_tau_wall,
                                                       rp_.score_allow_fallback,
                                                       coarse_ring,
                                                       coarse_ring,
                                                       coarse_ring,
                                                       hit_c,
                                                       &coarse_tier,
                                                       nullptr);
          } else if (rp_.use_score_filter) {
            ok = map.QueryNearestSurfelFilteredWithRing(pw,
                                                        rp_.score_tau_wall,
                                                        rp_.score_allow_fallback,
                                                        coarse_ring,
                                                        hit_c,
                                                        nullptr,
                                                        &coarse_passed_filter);
          } else {
            ok = map.QueryNearestSurfelWithRing(pw, coarse_ring, hit_c);
          }
          coarse_ok = ok;
          ok = false;

          if (coarse_ok) {
            coarse_hits += 1;
            const auto key = fine_map->KeyFromPoint(hit_c.mu);
            VoxelHashMap::SurfelHit hit_f;
            bool refine_ok =
                fine_map->QueryNearestSurfelAroundKey(
                    key, coarse_refine_ring, pw, hit_f);
            if (refine_ok) {
              coarse_refine_hits += 1;
              hit = hit_f;
              if (rp_.use_edge_first) {
                const bool edge_ok = hit.edge_valid && hit.edge_score >= rp_.tau_edge;
                const bool wall_ok = hit.score_valid && hit.score >= rp_.score_tau_wall;
                if (coarse_tier == VoxelHashMap::MatchTier::EDGE && edge_ok) {
                  hit.passed_score_filter = false;
                  hit.used_fallback = false;
                  ok = true;
                } else if (coarse_tier == VoxelHashMap::MatchTier::WALL && wall_ok) {
                  hit.passed_score_filter = true;
                  hit.used_fallback = false;
                  ok = true;
                } else if (coarse_tier == VoxelHashMap::MatchTier::FALLBACK) {
                  hit.passed_score_filter = false;
                  hit.used_fallback = true;
                  ok = true;
                }
              } else if (rp_.use_score_filter) {
                const bool wall_ok = hit.score_valid && hit.score >= rp_.score_tau_wall;
                if (coarse_passed_filter) {
                  if (wall_ok) {
                    ok = true;
                    used_fallback = false;
                    passed_filter = true;
                    hit.passed_score_filter = true;
                    hit.used_fallback = false;
                  }
                } else if (rp_.score_allow_fallback) {
                  ok = true;
                  used_fallback = !wall_ok;
                  passed_filter = wall_ok;
                  hit.passed_score_filter = wall_ok;
                  hit.used_fallback = !wall_ok;
                } else if (wall_ok) {
                  ok = true;
                  used_fallback = false;
                  passed_filter = true;
                  hit.passed_score_filter = true;
                  hit.used_fallback = false;
                }
              } else {
                ok = true;
                hit.passed_score_filter = hit.score_valid && hit.score >= rp_.score_tau_wall;
                hit.used_fallback = false;
              }
            }
          }

          if (!ok && coarse_fallback_full && fine_map) {
            coarse_fallback_hits += 1;
            ok = rp_.use_edge_first
                ? fine_map->QueryNearestHierarchical(pw,
                                                     rp_.tau_edge,
                                                     rp_.score_tau_wall,
                                                     rp_.score_allow_fallback,
                                                     hit,
                                                     nullptr)
                : (rp_.use_score_filter
                       ? fine_map->QueryNearestSurfelFiltered(pw,
                                                              rp_.score_tau_wall,
                                                              rp_.score_allow_fallback,
                                                              hit,
                                                              &used_fallback,
                                                              &passed_filter)
                       : fine_map->QueryNearestSurfel(pw, hit));
          }
        } else if (use_ring_query_effective) {
          // Two-stage ring query: ring=0 then ring=ring_fallback.
          const int r0 = 0;
          if (rp_.use_edge_first) {
            ok = map.QueryNearestHierarchicalWithRings(pw,
                                                       rp_.tau_edge,
                                                       rp_.score_tau_wall,
                                                       rp_.score_allow_fallback,
                                                       r0, r0, r0,
                                                       hit,
                                                       nullptr,
                                                       nullptr);
            if (ok) {
              query_edge_hits += 1;
              query_ring0_hits += 1;
            } else {
              ok = map.QueryNearestHierarchicalWithRings(pw,
                                                         rp_.tau_edge,
                                                         rp_.score_tau_wall,
                                                         rp_.score_allow_fallback,
                                                         ring_fallback,
                                                         ring_fallback,
                                                         ring_fallback,
                                                         hit,
                                                         nullptr,
                                                         nullptr);
              if (ok) query_fallback_hits += 1;
            }
          } else if (rp_.use_score_filter) {
            ok = map.QueryNearestSurfelFilteredWithRing(pw,
                                                        rp_.score_tau_wall,
                                                        false,
                                                        r0,
                                                        hit,
                                                        &used_fallback,
                                                        &passed_filter);
            if (ok) {
              query_edge_hits += 1;
              query_ring0_hits += 1;
            } else if (rp_.score_allow_fallback) {
              ok = map.QueryNearestSurfelWithRing(pw, ring_fallback, hit);
              if (ok) {
                query_fallback_hits += 1;
                used_fallback = true;
                passed_filter = false;
                hit.used_fallback = true;
                hit.passed_score_filter = false;
              }
            }
          } else {
            ok = map.QueryNearestSurfelWithRing(pw, r0, hit);
            if (ok) {
              query_edge_hits += 1;
              query_ring0_hits += 1;
            } else {
              ok = map.QueryNearestSurfelWithRing(pw, ring_fallback, hit);
              if (ok) query_fallback_hits += 1;
            }
          }
        } else if (skip_ring0) {
          skipped_ring0_queries += 1;
          ok = map.QueryNearestSurfelWithRing(pw, ring_fallback, hit);
          if (ok) {
            query_fallback_hits += 1;
          }
        } else {
          ok = rp_.use_edge_first
              ? map.QueryNearestHierarchical(pw,
                                             rp_.tau_edge,
                                             rp_.score_tau_wall,
                                             rp_.score_allow_fallback,
                                             hit,
                                             nullptr)
              : (rp_.use_score_filter
                     ? map.QueryNearestSurfelFiltered(pw,
                                                      rp_.score_tau_wall,
                                                      rp_.score_allow_fallback,
                                                      hit,
                                                      &used_fallback,
                                                      &passed_filter)
                     : map.QueryNearestSurfel(pw, hit));
        }
        if (!ok) continue;
        if (refine_with_fine && refine_map) {
          VoxelHashMap::SurfelHit fine_hit;
          if (refine_map->QueryNearestSurfelWithRing(pw, refine_ring, fine_hit)) {
            hit.mu = fine_hit.mu;
            hit.normal = fine_hit.normal;
            hit.normal_valid = fine_hit.normal_valid;
            refine_used += 1;
          }
        }
        if (!rp_.use_score_filter && !rp_.use_edge_first) {
          hit.passed_score_filter = hit.score_valid && hit.score >= rp_.score_tau_wall;
          hit.used_fallback = false;
        }
        if (rp_.use_edge_first) {
          if (hit.used_fallback) {
            fallback_used += 1;
          } else if (hit.passed_score_filter) {
            wall_used += 1;
          } else {
            edge_used += 1;
          }
        } else {
          if (rp_.use_score_filter && passed_filter && !used_fallback) highscore_used += 1;
          if (rp_.use_score_filter && used_fallback) fallback_used += 1;
        }

        const Eigen::Vector3d r = pw - hit.mu;
        const double d2 = r.squaredNorm();
        if (d2 > max_corr2) continue;

        double w = 1.0;
        bool boosted = false;
        if (rp_.use_score_weight) {
          const bool use_score = hit.score_valid &&
              (!rp_.weight_only_highscore || hit.passed_score_filter) &&
              !hit.used_fallback;
          if (use_score) {
            const double w_raw = w_base + w_gain * hit.score;
            w = std::clamp(w_raw, w_min, w_max);
            boosted = true;
          } else {
            w = w_base;
          }
        }
        corrs.push_back(Corr{
            pw, hit.mu, hit.normal, hit.normal_valid, d2, w, hit.score,
            hit.score_valid, hit.edge_score, hit.edge_valid,
            hit.passed_score_filter, hit.used_fallback});
        weights.push_back(w);
        if (boosted) weight_boost_used += 1;
        if (hit.score_valid) used_scores.push_back(hit.score);
      }
    }
    if (perf) {
      const auto t_corr1 = Clock::now();
      perf->dt_corr_build_ms += std::chrono::duration<double, std::milli>(t_corr1 - t_corr0).count();
    }

    if (!rp_.use_corr_budget) {
      // 2) enforce correspondence budget (pick smallest d2)
      const auto t_budget0 = perf ? Clock::now() : Clock::time_point{};
      if (max_corr > 0 &&
          static_cast<int>(corrs.size()) > max_corr) {
        auto mid = corrs.begin() + max_corr;
        std::nth_element(
            corrs.begin(), mid, corrs.end(),
            [](const Corr& a, const Corr& b) { return a.d2 < b.d2; });
        corrs.resize(max_corr);
      }
      if (perf) {
        const auto t_budget1 = Clock::now();
        perf->dt_budget_ms += std::chrono::duration<double, std::milli>(t_budget1 - t_budget0).count();
      }
      if (!use_ring_query_effective && !skip_ring0) {
        query_edge_hits = edge_used;
        query_wall_hits = wall_used;
        query_fallback_hits = fallback_used;
      }
    }

    const int used = static_cast<int>(corrs.size());
    if (used < rp_.min_effective_corr) {
      return false;
    }

    if (rp_.enable_single_solve_weighted) {
      std::vector<double> w_used;
      std::vector<int> w_types;
      w_used.reserve(corrs.size());
      w_types.reserve(corrs.size());

      std::size_t edge_used_w = 0;
      std::size_t wall_used_w = 0;
      std::size_t fallback_used_w = 0;
      double w_sum = 0.0;
      double w_min_used = 0.0;
      double w_max_used = 0.0;
      bool w_init = false;

      for (const auto& c : corrs) {
        const bool is_edge =
            rp_.use_edge_first && !c.used_fallback && !c.passed_score_filter;
        const bool is_wall =
            (!is_edge) && c.score_valid && (c.score >= rp_.score_tau_wall);
        const int type = is_edge ? 0 : (is_wall ? 1 : 2);
        w_types.push_back(type);
        if (type == 0) {
          edge_used_w += 1;
        } else if (type == 1) {
          wall_used_w += 1;
        } else {
          fallback_used_w += 1;
        }

        double w = c.w;
        const double w_cat = (type == 0)
            ? rp_.w_edge
            : (type == 1 ? rp_.w_wall : rp_.w_fallback);
        w *= w_cat;
        if (type == 0 && rp_.w_edge_auto && c.edge_valid) {
          w *= (1.0 + rp_.w_edge_gain * c.edge_score);
        }
        if (type == 1 && rp_.w_wall_auto && c.score_valid) {
          w *= (1.0 + rp_.w_wall_gain * c.score);
        }
        w = std::clamp(w, rp_.w_floor, rp_.w_cap);
        w_used.push_back(w);
        w_sum += w;
        if (!w_init) {
          w_min_used = w;
          w_max_used = w;
          w_init = true;
        } else {
          w_min_used = std::min(w_min_used, w);
          w_max_used = std::max(w_max_used, w);
        }
      }

      double w_mean_used = (w_used.empty()) ? 0.0
                                            : (w_sum / static_cast<double>(w_used.size()));
      if (rp_.weighted_normalize && w_mean_used > 0.0) {
        const double scale = 1.0 / w_mean_used;
        w_sum = 0.0;
        w_min_used = 0.0;
        w_max_used = 0.0;
        w_init = false;
        for (double& w : w_used) {
          w *= scale;
          w_sum += w;
          if (!w_init) {
            w_min_used = w;
            w_max_used = w;
            w_init = true;
          } else {
            w_min_used = std::min(w_min_used, w);
            w_max_used = std::max(w_max_used, w);
          }
        }
        w_mean_used = (w_used.empty()) ? 0.0
                                       : (w_sum / static_cast<double>(w_used.size()));
      }

      double w_edge_sum = 0.0;
      double w_wall_sum = 0.0;
      double w_fallback_sum = 0.0;
      for (std::size_t i = 0; i < w_used.size(); ++i) {
        const double w = w_used[i];
        const int type = w_types[i];
        if (type == 0) {
          w_edge_sum += w;
        } else if (type == 1) {
          w_wall_sum += w;
        } else {
          w_fallback_sum += w;
        }
      }
      const double w_edge_used_mean =
          (edge_used_w > 0) ? (w_edge_sum / static_cast<double>(edge_used_w)) : 0.0;
      const double w_wall_used_mean =
          (wall_used_w > 0) ? (w_wall_sum / static_cast<double>(wall_used_w)) : 0.0;
      const double w_fallback_used_mean =
          (fallback_used_w > 0) ? (w_fallback_sum / static_cast<double>(fallback_used_w)) : 0.0;

      Eigen::Matrix<double,6,6> H = Eigen::Matrix<double,6,6>::Zero();
      Eigen::Matrix<double,6,1> b = Eigen::Matrix<double,6,1>::Zero();

      double cost_u_sum = 0.0;
      double plane_abs_sum = 0.0;
      std::size_t plane_used = 0;

      const auto t_accum0 = perf ? Clock::now() : Clock::time_point{};
      for (std::size_t i = 0; i < corrs.size(); ++i) {
        const auto& c = corrs[i];
        const double w = w_used[i];
        const Eigen::Vector3d r = c.pw - c.q;

        Eigen::Matrix<double,3,6> J;
        J.setZero();
        J.block<3,3>(0,0).setIdentity();
        J.block<3,3>(0,3) = -Hat(c.pw);

        if (w_pt > 0.0) {
          const double ww = w_pt * w;
          H.noalias() += ww * (J.transpose() * J);
          b.noalias() += ww * (J.transpose() * r);
          cost_u_sum += w_pt * c.d2;
        }

        if (alpha > 0.0 && c.normal_valid) {
          Eigen::Matrix<double,1,6> Jpl;
          Jpl.block<1,3>(0,0) = c.normal.transpose();
          Jpl.block<1,3>(0,3) = -c.normal.transpose() * Hat(c.pw);

          const double r_pl = c.normal.dot(r);
          const double wp = rp_.weighted_plane ? w : 1.0;
          const double ww = alpha * wp;
          H.noalias() += ww * (Jpl.transpose() * Jpl);
          b.noalias() += ww * (Jpl.transpose() * r_pl);
          plane_abs_sum += std::abs(r_pl);
          plane_used += 1;
        }

        residuals.push_back(std::sqrt(c.d2));
        src_inliers.push_back(c.pw);
        tgt_inliers.push_back(c.q);
        influence_key.push_back(c.d2);
        weight_val.push_back(w);
      }
      if (perf) {
        const auto t_accum1 = Clock::now();
        perf->dt_accum_ms += std::chrono::duration<double, std::milli>(t_accum1 - t_accum0).count();
      }

      const double cost_u_now = cost_u_sum / static_cast<double>(used);

      if (topk_stats && !influence_key.empty() && this->topk_K > 0) {
        IcpTopKStats tk;
        tk.it = iter;

        const auto idx = TopKIndices(influence_key, this->topk_K);
        for (int i : idx) {
          tk.src_pts.push_back(src_inliers[i]);
          tk.tgt_pts.push_back(tgt_inliers[i]);
          tk.influence.push_back(influence_key[i]);
          tk.weight.push_back(weight_val[i]);
        }
        topk_stats->push_back(std::move(tk));
      }

      const auto t_solve0 = perf ? Clock::now() : Clock::time_point{};
      Eigen::LDLT<Eigen::Matrix<double,6,6>> ldlt(H);
      if (ldlt.info() != Eigen::Success) {
        return false;
      }

      Eigen::Matrix<double,6,1> dx = ldlt.solve(-b);
      if (perf) {
        const auto t_solve1 = Clock::now();
        perf->dt_solve_ms += std::chrono::duration<double, std::milli>(t_solve1 - t_solve0).count();
        perf->used_corr = static_cast<std::size_t>(used);
        perf->query_edge_hits = query_edge_hits;
        perf->query_wall_hits = query_wall_hits;
        perf->query_fallback_hits = query_fallback_hits;
        perf->query_ring_used_mean =
            (ring_used_cnt > 0) ? (ring_used_sum / static_cast<double>(ring_used_cnt)) : 0.0;
        perf->query_ring0_hits = query_ring0_hits;
        perf->coarse_hits = coarse_hits;
        perf->coarse_refine_hits = coarse_refine_hits;
        perf->coarse_fallback_hits = coarse_fallback_hits;
        perf->coarse_ring = enable_coarse_assoc ? coarse_ring : 0;
        perf->coarse_refine_ring = enable_coarse_assoc ? coarse_refine_ring : 0;
        if (attempts > 0) {
          perf->ring0_hit_ratio =
              static_cast<double>(query_ring0_hits) / static_cast<double>(attempts);
          perf->ring_fallback_ratio =
              static_cast<double>(query_fallback_hits) / static_cast<double>(attempts);
        } else {
          perf->ring0_hit_ratio = 0.0;
          perf->ring_fallback_ratio = 0.0;
        }
        perf->skipped_ring0_queries = skipped_ring0_queries;
        perf->refine_used = refine_used;
        perf->refine_used_ratio =
            (attempts > 0) ? (static_cast<double>(refine_used) / static_cast<double>(attempts))
                           : 0.0;
      }

      const double cond_JTJ = ConditionNumberJTJ(H);
      const double yaw_info = YawInfoSchur(H);
      double eig_min = last_eig_min;
      double eig_max = last_eig_max;
      int eig_rank = last_eig_rank;
      const double yaw_eps = 0.1 * M_PI / 180.0;
      if (cond_JTJ > 3e5 && std::abs(dx(5)) > yaw_eps) {
        Eigen::SelfAdjointEigenSolver<Eigen::Matrix<double,6,6>> es(H);
        if (es.info() == Eigen::Success) {
          const auto evals = es.eigenvalues();
          eig_min = evals.minCoeff();
          eig_max = evals.maxCoeff();
          constexpr double kEigRankEps = 1e-8;
          eig_rank = 0;
          for (int i = 0; i < evals.size(); ++i) {
            if (evals[i] > kEigRankEps) ++eig_rank;
          }
          last_eig_min = eig_min;
          last_eig_max = eig_max;
          last_eig_rank = eig_rank;
        }
      }
      const double yaw_raw = dx(5);
      bool yaw_clamp_triggered = false;
      if (rp_.yaw_clamp_deg > 0.0) {
        const double max_yaw = rp_.yaw_clamp_deg * M_PI / 180.0;
        dx(5) = std::clamp(dx(5), -max_yaw, +max_yaw);
        yaw_clamp_triggered = (dx(5) != yaw_raw);
      }
      const double yaw_clamped = dx(5);
      bool yaw_frozen = false;
      int yaw_freeze_reason = 0;
      if (rp_.legacy_yaw_freeze_by_cond) {
        if (cond_JTJ > 1e5) {
          dx(5) = 0.0;
          yaw_frozen = true;
          yaw_freeze_reason = 2;
        }
      } else {
        if (yaw_info < rp_.yaw_info_thresh) {
          dx(5) = 0.0;
          yaw_frozen = true;
          yaw_freeze_reason = 1;
        }
      }
      T = Sophus::SE3d::exp(dx) * T;

      if (iter_stats) {
        IcpIterStats st;
        st.it = iter;
        st.N = static_cast<std::size_t>(used);
        st.inlier_ratio =
            static_cast<double>(used) /
            static_cast<double>(scan_body.size());

        st.mean_res = Mean(residuals);
        st.median_res = Median(residuals);
        st.mad_res = MAD(residuals, st.median_res);
        st.cond_JTJ = cond_JTJ;
        st.grad_norm = b.norm();

        st.yaw_raw = yaw_raw;
        st.yaw_clamped = yaw_clamped;
        st.yaw_clamp_triggered = yaw_clamp_triggered ? 1 : 0;
        st.yaw_frozen = yaw_frozen ? 1 : 0;
        st.eig_min = eig_min;
        st.eig_max = eig_max;
        st.eig_rank = eig_rank;
        st.yaw_info = yaw_info;
        st.yaw_freeze_reason = yaw_freeze_reason;
        st.cond_JTJ_scaled = cond_JTJ;
        st.used_corr_total = static_cast<std::size_t>(used);
        st.corr_edge_used = edge_used_w;
        st.corr_wall_used = wall_used_w;
        st.corr_fallback_used = fallback_used_w;
        if (rp_.use_corr_budget && max_corr > 0) {
          st.budget_edge = budget_edge;
          st.budget_wall = budget_wall;
          st.budget_fallback_eff =
              std::max(0, max_corr - static_cast<int>(edge_used) - static_cast<int>(wall_used));
          st.corr_build_early_stop = corr_build_early_stop;
          st.corr_budget_downgrade = corr_budget_downgrade;
          st.ring_query_downgrade = ring_query_downgrade;
        } else {
          st.budget_edge = 0;
          st.budget_wall = 0;
          st.budget_fallback_eff = used;
          st.corr_build_early_stop = 0;
          st.corr_budget_downgrade = 0;
          st.ring_query_downgrade = 0;
        }
        st.corr_queried_points = attempts;

        st.alpha_p2plane = alpha;
        st.plane_used_ratio =
            (used > 0) ? (static_cast<double>(plane_used) / static_cast<double>(used)) : 0.0;
        st.plane_res_mean =
            (plane_used > 0) ? (plane_abs_sum / static_cast<double>(plane_used)) : 0.0;
        st.corr_edge_ratio =
            (used > 0) ? (static_cast<double>(edge_used_w) / static_cast<double>(used)) : 0.0;
        st.corr_wall_ratio =
            (used > 0) ? (static_cast<double>(wall_used_w) / static_cast<double>(used)) : 0.0;
        st.corr_fallback_ratio =
            (used > 0) ? (static_cast<double>(fallback_used_w) / static_cast<double>(used)) : 0.0;
        st.tau_edge = rp_.tau_edge;
        st.edge_first_used = rp_.use_edge_first ? 1 : 0;
        if (rp_.use_edge_first) {
          st.corr_highscore_ratio = st.corr_wall_ratio;
          st.highscore_fallback_ratio =
              (attempts > 0)
                  ? (static_cast<double>(fallback_used_w) / static_cast<double>(attempts))
                  : 0.0;
        } else {
          st.corr_highscore_ratio =
              (used > 0) ? (static_cast<double>(highscore_used) / static_cast<double>(used)) : 0.0;
          st.highscore_fallback_ratio =
              (attempts > 0)
                  ? (static_cast<double>(fallback_used) / static_cast<double>(attempts))
                  : 0.0;
        }

        st.score_tau_wall = rp_.score_tau_wall;
        st.score_filter_used = rp_.use_score_filter ? 1 : 0;

        st.w_mean = w_mean_used;
        st.w_min = w_min_used;
        st.w_max = w_max_used;
        if (!w_used.empty()) {
          const std::size_t idx = static_cast<std::size_t>(
              std::max<std::size_t>(1, (w_used.size() * 9 + 9) / 10)) - 1;
          std::vector<double> wtmp = w_used;
          std::nth_element(wtmp.begin(), wtmp.begin() + idx, wtmp.end());
          st.w_p90 = wtmp[idx];
        }
        st.w_highscore_ratio =
            (w_used.size() > 0)
                ? (static_cast<double>(weight_boost_used) / static_cast<double>(w_used.size()))
                : 0.0;
        if (!used_scores.empty()) {
          double sum_s = 0.0;
          for (double s : used_scores) sum_s += s;
          st.score_mean_used = sum_s / static_cast<double>(used_scores.size());
        }
        st.weighted_enabled = 1;
        st.w_edge = rp_.w_edge;
        st.w_wall = rp_.w_wall;
        st.w_fallback = rp_.w_fallback;
        st.w_cap = rp_.w_cap;
        st.w_mean_used = w_mean_used;
        st.w_min_used = w_min_used;
        st.w_max_used = w_max_used;
        st.w_edge_used_mean = w_edge_used_mean;
        st.w_wall_used_mean = w_wall_used_mean;
        st.w_fallback_used_mean = w_fallback_used_mean;
        st.cost_u = cost_u_now;
        if (iter > 0 && std::isfinite(cost_u_prev) && cost_u_prev > 0.0) {
          st.rel_drop_u = std::abs(cost_u_prev - cost_u_now) / cost_u_prev;
        } else {
          st.rel_drop_u = 0.0;
        }
        st.stop_by_cost_u = 0;

        iter_stats->push_back(st);
      }

      if (dx.norm() < rp_.eps_dx) break;
      bool stop_by_cost_u = false;
      if (iter > 0 && std::isfinite(cost_u_prev) && cost_u_prev > 0.0) {
        const double rel_drop_u = std::abs(cost_u_prev - cost_u_now) / cost_u_prev;
        if (!rp_.enable_easy_stop_guard || iter >= rp_.easy_stop_min_iters) {
          if (rel_drop_u < rp_.eps_cost_rel) {
            stop_by_cost_u = true;
          }
        }
      }
      if (stop_by_cost_u) {
        if (iter_stats && !iter_stats->empty()) {
          auto& st = iter_stats->back();
          st.rel_drop_u =
              (iter > 0 && std::isfinite(cost_u_prev) && cost_u_prev > 0.0)
                  ? (std::abs(cost_u_prev - cost_u_now) / cost_u_prev)
                  : 0.0;
          st.stop_by_cost_u = 1;
        }
        break;
      }
      cost_u_prev = cost_u_now;
      continue;
    } else if (rp_.enable_landmark_first) {
      std::vector<Corr> corrs_edge;
      std::vector<Corr> corrs_wall;
      std::vector<Corr> corrs_fallback;
      corrs_edge.reserve(corrs.size());
      corrs_wall.reserve(corrs.size());
      corrs_fallback.reserve(corrs.size());

      for (const auto& c : corrs) {
        const bool is_edge =
            rp_.use_edge_first && !c.used_fallback && !c.passed_score_filter;
        const bool is_wall =
            (!is_edge) && c.score_valid && (c.score >= rp_.score_tau_wall);
        if (is_edge) {
          corrs_edge.push_back(c);
        } else if (is_wall) {
          corrs_wall.push_back(c);
        } else {
          corrs_fallback.push_back(c);
        }
      }

      std::vector<Corr> corrs_lm;
      corrs_lm.reserve(corrs_edge.size() + corrs_wall.size());
      corrs_lm.insert(corrs_lm.end(), corrs_edge.begin(), corrs_edge.end());
      corrs_lm.insert(corrs_lm.end(), corrs_wall.begin(), corrs_wall.end());

      bool lm_stage_used =
          static_cast<int>(corrs_lm.size()) >= rp_.landmark_stage_min_corr;
      int refine_mode = rp_.refine_stage_mode;
      if (!lm_stage_used && refine_mode == 0) {
        refine_mode = 2;
      }

      auto AccumulateHB =
          [&](const std::vector<Corr>& list,
              bool use_p2plane,
              double weight_scale,
              Eigen::Matrix<double,6,6>& H,
              Eigen::Matrix<double,6,1>& b,
              double& cost_sum,
              std::size_t& plane_used,
              double& plane_abs_sum,
              bool fill_stats) {
            const double alpha_stage = use_p2plane ? alpha : 0.0;
            const double w_pt_stage = 1.0 - alpha_stage;
            for (const auto& c : list) {
              const Eigen::Vector3d r = c.pw - c.q;

              Eigen::Matrix<double,3,6> J;
              J.setZero();
              J.block<3,3>(0,0).setIdentity();
              J.block<3,3>(0,3) = -Hat(c.pw);

              if (w_pt_stage > 0.0) {
                const double w = weight_scale * w_pt_stage * c.w;
                H.noalias() += w * (J.transpose() * J);
                b.noalias() += w * (J.transpose() * r);
                cost_sum += w * c.d2;
              }

              if (alpha_stage > 0.0 && c.normal_valid) {
                Eigen::Matrix<double,1,6> Jpl;
                Jpl.block<1,3>(0,0) = c.normal.transpose();
                Jpl.block<1,3>(0,3) = -c.normal.transpose() * Hat(c.pw);

                const double r_pl = c.normal.dot(r);
                const double w = weight_scale * alpha_stage * c.w;
                H.noalias() += w * (Jpl.transpose() * Jpl);
                b.noalias() += w * (Jpl.transpose() * r_pl);
                cost_sum += w * (r_pl * r_pl);
                plane_abs_sum += std::abs(r_pl);
                plane_used += 1;
              }

              if (fill_stats) {
                residuals.push_back(std::sqrt(c.d2));
                src_inliers.push_back(c.pw);
                tgt_inliers.push_back(c.q);
                influence_key.push_back(c.d2);
                weight_val.push_back(c.w);
              }
            }
          };

      auto ApplyYawPolicy =
          [&](Eigen::Matrix<double,6,1>& dx,
              const Eigen::Matrix<double,6,6>& H,
              double& yaw_raw,
              double& yaw_clamped,
              bool& yaw_clamp_triggered,
              bool& yaw_frozen,
              int& yaw_freeze_reason,
              double& cond_JTJ,
              double& yaw_info,
              double& eig_min,
              double& eig_max,
              int& eig_rank) {
            cond_JTJ = ConditionNumberJTJ(H);
            yaw_info = YawInfoSchur(H);
            eig_min = last_eig_min;
            eig_max = last_eig_max;
            eig_rank = last_eig_rank;
            const double yaw_eps = 0.1 * M_PI / 180.0;
            if (cond_JTJ > 3e5 && std::abs(dx(5)) > yaw_eps) {
              Eigen::SelfAdjointEigenSolver<Eigen::Matrix<double,6,6>> es(H);
              if (es.info() == Eigen::Success) {
                const auto evals = es.eigenvalues();
                eig_min = evals.minCoeff();
                eig_max = evals.maxCoeff();
                constexpr double kEigRankEps = 1e-8;
                eig_rank = 0;
                for (int i = 0; i < evals.size(); ++i) {
                  if (evals[i] > kEigRankEps) ++eig_rank;
                }
                last_eig_min = eig_min;
                last_eig_max = eig_max;
                last_eig_rank = eig_rank;
              }
            }
            yaw_raw = dx(5);
            yaw_clamp_triggered = false;
            if (rp_.yaw_clamp_deg > 0.0) {
              const double max_yaw = rp_.yaw_clamp_deg * M_PI / 180.0;
              dx(5) = std::clamp(dx(5), -max_yaw, +max_yaw);
              yaw_clamp_triggered = (dx(5) != yaw_raw);
            }
            yaw_clamped = dx(5);
            yaw_frozen = false;
            yaw_freeze_reason = 0;
            if (rp_.legacy_yaw_freeze_by_cond) {
              if (cond_JTJ > 1e5) {
                dx(5) = 0.0;
                yaw_frozen = true;
                yaw_freeze_reason = 2;
              }
            } else {
              if (yaw_info < rp_.yaw_info_thresh) {
                dx(5) = 0.0;
                yaw_frozen = true;
                yaw_freeze_reason = 1;
              }
            }
          };

      Eigen::Matrix<double,6,6> H_lm = Eigen::Matrix<double,6,6>::Zero();
      Eigen::Matrix<double,6,1> b_lm = Eigen::Matrix<double,6,1>::Zero();
      double cost_sum_lm = 0.0;
      std::size_t plane_used_lm = 0;
      double plane_abs_sum_lm = 0.0;
      Eigen::Matrix<double,6,1> dx_lm = Eigen::Matrix<double,6,1>::Zero();
      double yaw_raw_lm = 0.0;
      double yaw_clamped_lm = 0.0;
      bool yaw_clamp_triggered_lm = false;
      bool yaw_frozen_lm = false;
      int yaw_freeze_reason_lm = 0;
      double cond_JTJ_lm = 0.0;
      double yaw_info_lm = 0.0;
      double eig_min_lm = last_eig_min;
      double eig_max_lm = last_eig_max;
      int eig_rank_lm = last_eig_rank;
      int lm_corr_used = 0;

      const bool stage_a_is_final = (lm_stage_used && refine_mode == 0);
      if (lm_stage_used) {
        if (rp_.landmark_stage_max_corr > 0 &&
            static_cast<int>(corrs_lm.size()) > rp_.landmark_stage_max_corr) {
          auto mid = corrs_lm.begin() + rp_.landmark_stage_max_corr;
          std::nth_element(corrs_lm.begin(), mid, corrs_lm.end(),
                           [](const Corr& a, const Corr& b) { return a.d2 < b.d2; });
          corrs_lm.resize(rp_.landmark_stage_max_corr);
        }
        lm_corr_used = static_cast<int>(corrs_lm.size());
        const auto t_accum0 = perf ? Clock::now() : Clock::time_point{};
        AccumulateHB(corrs_lm,
                     rp_.landmark_stage_use_p2plane,
                     rp_.landmark_stage_weight,
                     H_lm,
                     b_lm,
                     cost_sum_lm,
                     plane_used_lm,
                     plane_abs_sum_lm,
                     stage_a_is_final);
        if (perf) {
          const auto t_accum1 = Clock::now();
          perf->dt_accum_ms +=
              std::chrono::duration<double, std::milli>(t_accum1 - t_accum0).count();
        }

        const auto t_solve0 = perf ? Clock::now() : Clock::time_point{};
        Eigen::LDLT<Eigen::Matrix<double,6,6>> ldlt(H_lm);
        if (ldlt.info() != Eigen::Success) {
          return false;
        }
        dx_lm = ldlt.solve(-b_lm);
        if (perf) {
          const auto t_solve1 = Clock::now();
          perf->dt_solve_ms +=
              std::chrono::duration<double, std::milli>(t_solve1 - t_solve0).count();
        }
        ApplyYawPolicy(dx_lm,
                       H_lm,
                       yaw_raw_lm,
                       yaw_clamped_lm,
                       yaw_clamp_triggered_lm,
                       yaw_frozen_lm,
                       yaw_freeze_reason_lm,
                       cond_JTJ_lm,
                       yaw_info_lm,
                       eig_min_lm,
                       eig_max_lm,
                       eig_rank_lm);
        T = Sophus::SE3d::exp(dx_lm) * T;
      }

      std::vector<Corr> corrs_refine;
      if (refine_mode == 1) {
        corrs_refine = corrs_fallback;
        if (corrs_refine.empty()) {
          corrs_refine = corrs;
        }
      } else if (refine_mode == 2) {
        corrs_refine = corrs;
      }

      bool refine_stage_used = false;
      int refine_corr_used = 0;
      Eigen::Matrix<double,6,6> H_ref = Eigen::Matrix<double,6,6>::Zero();
      Eigen::Matrix<double,6,1> b_ref = Eigen::Matrix<double,6,1>::Zero();
      double cost_sum_ref = 0.0;
      std::size_t plane_used_ref = 0;
      double plane_abs_sum_ref = 0.0;
      Eigen::Matrix<double,6,1> dx_ref = Eigen::Matrix<double,6,1>::Zero();
      double yaw_raw_ref = 0.0;
      double yaw_clamped_ref = 0.0;
      bool yaw_clamp_triggered_ref = false;
      bool yaw_frozen_ref = false;
      int yaw_freeze_reason_ref = 0;
      double cond_JTJ_ref = 0.0;
      double yaw_info_ref = 0.0;
      double eig_min_ref = last_eig_min;
      double eig_max_ref = last_eig_max;
      int eig_rank_ref = last_eig_rank;

      if (refine_mode != 0 && !corrs_refine.empty()) {
        if (rp_.refine_stage_max_corr > 0 &&
            static_cast<int>(corrs_refine.size()) > rp_.refine_stage_max_corr) {
          auto mid = corrs_refine.begin() + rp_.refine_stage_max_corr;
          std::nth_element(corrs_refine.begin(), mid, corrs_refine.end(),
                           [](const Corr& a, const Corr& b) { return a.d2 < b.d2; });
          corrs_refine.resize(rp_.refine_stage_max_corr);
        }
        refine_corr_used = static_cast<int>(corrs_refine.size());
        const auto t_accum0 = perf ? Clock::now() : Clock::time_point{};
        AccumulateHB(corrs_refine,
                     rp_.refine_stage_use_p2plane,
                     1.0,
                     H_ref,
                     b_ref,
                     cost_sum_ref,
                     plane_used_ref,
                     plane_abs_sum_ref,
                     true);
        if (perf) {
          const auto t_accum1 = Clock::now();
          perf->dt_accum_ms +=
              std::chrono::duration<double, std::milli>(t_accum1 - t_accum0).count();
        }

        const auto t_solve0 = perf ? Clock::now() : Clock::time_point{};
        Eigen::LDLT<Eigen::Matrix<double,6,6>> ldlt(H_ref);
        if (ldlt.info() != Eigen::Success) {
          return false;
        }
        dx_ref = ldlt.solve(-b_ref);
        if (perf) {
          const auto t_solve1 = Clock::now();
          perf->dt_solve_ms +=
              std::chrono::duration<double, std::milli>(t_solve1 - t_solve0).count();
        }
        ApplyYawPolicy(dx_ref,
                       H_ref,
                       yaw_raw_ref,
                       yaw_clamped_ref,
                       yaw_clamp_triggered_ref,
                       yaw_frozen_ref,
                       yaw_freeze_reason_ref,
                       cond_JTJ_ref,
                       yaw_info_ref,
                       eig_min_ref,
                       eig_max_ref,
                       eig_rank_ref);
        T = Sophus::SE3d::exp(dx_ref) * T;
        refine_stage_used = true;
      }

      const bool final_from_refine = refine_stage_used;
      const int used_final =
          final_from_refine ? refine_corr_used : lm_corr_used;
      const double cost_now =
          final_from_refine
              ? (refine_corr_used > 0
                     ? cost_sum_ref / static_cast<double>(refine_corr_used)
                     : 0.0)
              : (lm_corr_used > 0
                     ? cost_sum_lm / static_cast<double>(lm_corr_used)
                     : 0.0);

      if (perf) {
        perf->used_corr = static_cast<std::size_t>(used_final);
        perf->query_edge_hits = query_edge_hits;
        perf->query_wall_hits = query_wall_hits;
        perf->query_fallback_hits = query_fallback_hits;
        perf->query_ring_used_mean =
            (ring_used_cnt > 0) ? (ring_used_sum / static_cast<double>(ring_used_cnt)) : 0.0;
        perf->query_ring0_hits = query_ring0_hits;
        if (attempts > 0) {
          perf->ring0_hit_ratio =
              static_cast<double>(query_ring0_hits) / static_cast<double>(attempts);
          perf->ring_fallback_ratio =
              static_cast<double>(query_fallback_hits) / static_cast<double>(attempts);
        } else {
          perf->ring0_hit_ratio = 0.0;
          perf->ring_fallback_ratio = 0.0;
        }
        perf->skipped_ring0_queries = skipped_ring0_queries;
        perf->refine_used = refine_used;
        perf->refine_used_ratio =
            (attempts > 0) ? (static_cast<double>(refine_used) / static_cast<double>(attempts))
                           : 0.0;
      }

      // ---- Top-K statistics ----
      if (topk_stats && !influence_key.empty() && this->topk_K > 0) {
        IcpTopKStats tk;
        tk.it = iter;

        const auto idx = TopKIndices(influence_key, this->topk_K);
        for (int i : idx) {
          tk.src_pts.push_back(src_inliers[i]);
          tk.tgt_pts.push_back(tgt_inliers[i]);
          tk.influence.push_back(influence_key[i]);
          tk.weight.push_back(weight_val[i]);
        }
        topk_stats->push_back(std::move(tk));
      }

      const bool final_use_p2plane =
          final_from_refine ? rp_.refine_stage_use_p2plane
                            : rp_.landmark_stage_use_p2plane;
      const double alpha_final = final_use_p2plane ? alpha : 0.0;
      const std::size_t plane_used_final =
          final_from_refine ? plane_used_ref : plane_used_lm;
      const double plane_abs_sum_final =
          final_from_refine ? plane_abs_sum_ref : plane_abs_sum_lm;
      const double cond_JTJ_final =
          final_from_refine ? cond_JTJ_ref : cond_JTJ_lm;
      const double yaw_info_final =
          final_from_refine ? yaw_info_ref : yaw_info_lm;
      const double yaw_raw_final =
          final_from_refine ? yaw_raw_ref : yaw_raw_lm;
      const double yaw_clamped_final =
          final_from_refine ? yaw_clamped_ref : yaw_clamped_lm;
      const bool yaw_clamp_triggered_final =
          final_from_refine ? yaw_clamp_triggered_ref : yaw_clamp_triggered_lm;
      const bool yaw_frozen_final =
          final_from_refine ? yaw_frozen_ref : yaw_frozen_lm;
      const int yaw_freeze_reason_final =
          final_from_refine ? yaw_freeze_reason_ref : yaw_freeze_reason_lm;
      const double eig_min_final =
          final_from_refine ? eig_min_ref : eig_min_lm;
      const double eig_max_final =
          final_from_refine ? eig_max_ref : eig_max_lm;
      const int eig_rank_final =
          final_from_refine ? eig_rank_ref : eig_rank_lm;
      const Eigen::Matrix<double,6,1> dx_final =
          final_from_refine ? dx_ref : dx_lm;

      // ---- Iter statistics ----
      if (iter_stats) {
        IcpIterStats st;
        st.it = iter;
        st.N = static_cast<std::size_t>(used_final);
        st.inlier_ratio =
            static_cast<double>(used_final) /
            static_cast<double>(scan_body.size());

        st.mean_res = Mean(residuals);
        st.median_res = Median(residuals);
        st.mad_res = MAD(residuals, st.median_res);
        st.cond_JTJ = cond_JTJ_final;
        st.grad_norm = final_from_refine ? b_ref.norm() : b_lm.norm();

        st.yaw_raw = yaw_raw_final;
        st.yaw_clamped = yaw_clamped_final;
        st.yaw_clamp_triggered = yaw_clamp_triggered_final ? 1 : 0;
        st.yaw_frozen = yaw_frozen_final ? 1 : 0;
        st.eig_min = eig_min_final;
        st.eig_max = eig_max_final;
        st.eig_rank = eig_rank_final;
        st.yaw_info = yaw_info_final;
        st.yaw_freeze_reason = yaw_freeze_reason_final;
        st.cond_JTJ_scaled = cond_JTJ_final;
        st.used_corr_total = static_cast<std::size_t>(used);
        if (rp_.use_corr_budget && max_corr > 0) {
          st.budget_edge = budget_edge;
          st.budget_wall = budget_wall;
          st.budget_fallback_eff =
              std::max(0, max_corr - static_cast<int>(edge_used) - static_cast<int>(wall_used));
          st.corr_edge_used = edge_used;
          st.corr_wall_used = wall_used;
          st.corr_fallback_used = fallback_used;
          st.corr_build_early_stop = corr_build_early_stop;
          st.corr_budget_downgrade = corr_budget_downgrade;
          st.ring_query_downgrade = ring_query_downgrade;
        } else {
          st.budget_edge = 0;
          st.budget_wall = 0;
          st.budget_fallback_eff = used;
          st.corr_edge_used = 0;
          st.corr_wall_used = 0;
          st.corr_fallback_used = used;
          st.corr_build_early_stop = 0;
          st.corr_budget_downgrade = 0;
          st.ring_query_downgrade = 0;
        }
        st.corr_queried_points = attempts;

        st.alpha_p2plane = alpha_final;
        st.plane_used_ratio =
            (used_final > 0)
                ? (static_cast<double>(plane_used_final) / static_cast<double>(used_final))
                : 0.0;
        st.plane_res_mean =
            (plane_used_final > 0)
                ? (plane_abs_sum_final / static_cast<double>(plane_used_final))
                : 0.0;
        if (rp_.use_edge_first) {
          st.corr_edge_ratio =
              (used > 0) ? (static_cast<double>(edge_used) / static_cast<double>(used)) : 0.0;
          st.corr_wall_ratio =
              (used > 0) ? (static_cast<double>(wall_used) / static_cast<double>(used)) : 0.0;
          st.corr_fallback_ratio =
              (used > 0) ? (static_cast<double>(fallback_used) / static_cast<double>(used)) : 0.0;
          st.tau_edge = rp_.tau_edge;
          st.edge_first_used = 1;
          st.corr_highscore_ratio = st.corr_wall_ratio;
          st.highscore_fallback_ratio =
              (attempts > 0)
                  ? (static_cast<double>(fallback_used) / static_cast<double>(attempts))
                  : 0.0;
        } else {
          st.corr_highscore_ratio =
              (used > 0) ? (static_cast<double>(highscore_used) / static_cast<double>(used)) : 0.0;
          st.highscore_fallback_ratio =
              (attempts > 0)
                  ? (static_cast<double>(fallback_used) / static_cast<double>(attempts))
                  : 0.0;
          st.edge_first_used = 0;
        }
        st.score_tau_wall = rp_.score_tau_wall;
        st.score_filter_used = rp_.use_score_filter ? 1 : 0;

        weights.clear();
        used_scores.clear();
        weight_boost_used = 0;
        const auto& final_corrs = final_from_refine ? corrs_refine : corrs_lm;
        for (const auto& c : final_corrs) {
          weights.push_back(c.w);
          if (c.score_valid) used_scores.push_back(c.score);
          if (c.w > w_base + 1e-12) weight_boost_used += 1;
        }

        if (!weights.empty()) {
          double sum_w = 0.0;
          double min_w = weights[0];
          double max_w = weights[0];
          for (double w : weights) {
            sum_w += w;
            min_w = std::min(min_w, w);
            max_w = std::max(max_w, w);
          }
          st.w_mean = sum_w / static_cast<double>(weights.size());
          st.w_min = min_w;
          st.w_max = max_w;
          const std::size_t idx = static_cast<std::size_t>(
              std::max<std::size_t>(1, (weights.size() * 9 + 9) / 10)) - 1;
          std::vector<double> wtmp = weights;
          std::nth_element(wtmp.begin(), wtmp.begin() + idx, wtmp.end());
          st.w_p90 = wtmp[idx];
          st.w_highscore_ratio =
              (weights.size() > 0)
                  ? (static_cast<double>(weight_boost_used) / static_cast<double>(weights.size()))
                  : 0.0;
        }
        if (!used_scores.empty()) {
          double sum_s = 0.0;
          for (double s : used_scores) sum_s += s;
          st.score_mean_used = sum_s / static_cast<double>(used_scores.size());
        }

        st.lm_first_enabled = 1;
        st.lm_stage_used = lm_stage_used ? 1 : 0;
        st.lm_corr_used = lm_corr_used;
        st.refine_stage_mode = refine_mode;
        st.refine_corr_used = refine_corr_used;
        st.lm_dx_norm = dx_lm.norm();
        st.refine_dx_norm = dx_ref.norm();
        st.lm_cost =
            (lm_corr_used > 0)
                ? (cost_sum_lm / static_cast<double>(lm_corr_used))
                : 0.0;
        st.refine_cost =
            (refine_corr_used > 0)
                ? (cost_sum_ref / static_cast<double>(refine_corr_used))
                : 0.0;
        st.lm_plane_used_ratio =
            (lm_corr_used > 0)
                ? (static_cast<double>(plane_used_lm) / static_cast<double>(lm_corr_used))
                : 0.0;
        st.refine_plane_used_ratio =
            (refine_corr_used > 0)
                ? (static_cast<double>(plane_used_ref) / static_cast<double>(refine_corr_used))
                : 0.0;
        st.cost_u = 0.0;
        st.rel_drop_u = 0.0;
        st.stop_by_cost_u = 0;

        iter_stats->push_back(st);
      }

      if (dx_final.norm() < rp_.eps_dx) break;
      if (iter > 0 && std::isfinite(cost_prev) && cost_prev > 0.0) {
        const double rel_drop = std::abs(cost_prev - cost_now) / cost_prev;
        if (rel_drop < rp_.eps_cost_rel) break;
      }
      cost_prev = cost_now;
      continue;
    }

    // 3) accumulate normal equations on used correspondences
    Eigen::Matrix<double,6,6> H = Eigen::Matrix<double,6,6>::Zero();
    Eigen::Matrix<double,6,1> b = Eigen::Matrix<double,6,1>::Zero();

    double cost_sum = 0.0;
    double plane_abs_sum = 0.0;
    std::size_t plane_used = 0;

    const auto t_accum0 = perf ? Clock::now() : Clock::time_point{};
    for (const auto& c : corrs) {
      const Eigen::Vector3d r = c.pw - c.q;

      Eigen::Matrix<double,3,6> J;
      J.setZero();
      J.block<3,3>(0,0).setIdentity();
      J.block<3,3>(0,3) = -Hat(c.pw);

      if (w_pt > 0.0) {
        const double w = w_pt * c.w;
        H.noalias() += w * (J.transpose() * J);
        b.noalias() += w * (J.transpose() * r);
        cost_sum += w * c.d2;
      }

      if (alpha > 0.0 && c.normal_valid) {
        Eigen::Matrix<double,1,6> Jpl;
        Jpl.block<1,3>(0,0) = c.normal.transpose();
        Jpl.block<1,3>(0,3) = -c.normal.transpose() * Hat(c.pw);

        const double r_pl = c.normal.dot(r);
        const double w = alpha * c.w;
        H.noalias() += w * (Jpl.transpose() * Jpl);
        b.noalias() += w * (Jpl.transpose() * r_pl);
        cost_sum += w * (r_pl * r_pl);
        plane_abs_sum += std::abs(r_pl);
        plane_used += 1;
      }

      residuals.push_back(std::sqrt(c.d2));

      // stats buffers
      src_inliers.push_back(c.pw);
      tgt_inliers.push_back(c.q);
      influence_key.push_back(c.d2);
      weight_val.push_back(c.w);
    }
    if (perf) {
      const auto t_accum1 = Clock::now();
      perf->dt_accum_ms += std::chrono::duration<double, std::milli>(t_accum1 - t_accum0).count();
    }

    const double cost_now = cost_sum / static_cast<double>(used);

    // ---- Top-K statistics ----
    if (topk_stats && !influence_key.empty() && this->topk_K > 0) {
      IcpTopKStats tk;
      tk.it = iter;

      const auto idx = TopKIndices(influence_key, this->topk_K);
      for (int i : idx) {
        tk.src_pts.push_back(src_inliers[i]);
        tk.tgt_pts.push_back(tgt_inliers[i]);
        tk.influence.push_back(influence_key[i]);
        tk.weight.push_back(weight_val[i]);
      }
      topk_stats->push_back(std::move(tk));
    }

    // Solve normal equation
    const auto t_solve0 = perf ? Clock::now() : Clock::time_point{};
    Eigen::LDLT<Eigen::Matrix<double,6,6>> ldlt(H);
    if (ldlt.info() != Eigen::Success) {
      return false;
    }

    Eigen::Matrix<double,6,1> dx = ldlt.solve(-b);
    if (perf) {
      const auto t_solve1 = Clock::now();
      perf->dt_solve_ms += std::chrono::duration<double, std::milli>(t_solve1 - t_solve0).count();
      perf->used_corr = static_cast<std::size_t>(used);
      perf->query_edge_hits = query_edge_hits;
      perf->query_wall_hits = query_wall_hits;
      perf->query_fallback_hits = query_fallback_hits;
      perf->query_ring_used_mean =
          (ring_used_cnt > 0) ? (ring_used_sum / static_cast<double>(ring_used_cnt)) : 0.0;
      perf->query_ring0_hits = query_ring0_hits;
      perf->coarse_hits = coarse_hits;
      perf->coarse_refine_hits = coarse_refine_hits;
      perf->coarse_fallback_hits = coarse_fallback_hits;
      perf->coarse_ring = enable_coarse_assoc ? coarse_ring : 0;
      perf->coarse_refine_ring = enable_coarse_assoc ? coarse_refine_ring : 0;
      if (attempts > 0) {
        perf->ring0_hit_ratio =
            static_cast<double>(query_ring0_hits) / static_cast<double>(attempts);
        perf->ring_fallback_ratio =
            static_cast<double>(query_fallback_hits) / static_cast<double>(attempts);
      } else {
        perf->ring0_hit_ratio = 0.0;
        perf->ring_fallback_ratio = 0.0;
      }
      perf->skipped_ring0_queries = skipped_ring0_queries;
      perf->refine_used = refine_used;
      perf->refine_used_ratio =
          (attempts > 0) ? (static_cast<double>(refine_used) / static_cast<double>(attempts))
                         : 0.0;
    }
    const double cond_JTJ = ConditionNumberJTJ(H);
    const double yaw_info = YawInfoSchur(H);
    double eig_min = last_eig_min;
    double eig_max = last_eig_max;
    int eig_rank = last_eig_rank;
    const double yaw_eps = 0.1 * M_PI / 180.0;
    if (cond_JTJ > 3e5 && std::abs(dx(5)) > yaw_eps) {
      Eigen::SelfAdjointEigenSolver<Eigen::Matrix<double,6,6>> es(H);
      if (es.info() == Eigen::Success) {
        const auto evals = es.eigenvalues();
        eig_min = evals.minCoeff();
        eig_max = evals.maxCoeff();
        constexpr double kEigRankEps = 1e-8;
        eig_rank = 0;
        for (int i = 0; i < evals.size(); ++i) {
          if (evals[i] > kEigRankEps) ++eig_rank;
        }
        last_eig_min = eig_min;
        last_eig_max = eig_max;
        last_eig_rank = eig_rank;
      }
    }
    // Optional yaw clamp: dx = [tx,ty,tz, wx,wy,wz] in Sophus (upsilon, omega).
    // We treat wz as yaw update around +Z for small-angle increments.
    // --- yaw clamp (per-iteration) ---
    const double yaw_raw = dx(5);
    bool yaw_clamp_triggered = false;
    if (rp_.yaw_clamp_deg > 0.0) {
      const double max_yaw = rp_.yaw_clamp_deg * M_PI / 180.0;
      dx(5) = std::clamp(dx(5), -max_yaw, +max_yaw);  // omega_z
      yaw_clamp_triggered = (dx(5) != yaw_raw);
    }
    const double yaw_clamped = dx(5);
    bool yaw_frozen = false;
    int yaw_freeze_reason = 0;
    if (rp_.legacy_yaw_freeze_by_cond) {
      if (cond_JTJ > 1e5) {
        dx(5) = 0.0;
        yaw_frozen = true;
        yaw_freeze_reason = 2;
      }
    } else {
      if (yaw_info < rp_.yaw_info_thresh) {
        dx(5) = 0.0;
        yaw_frozen = true;
        yaw_freeze_reason = 1;
      }
    }
    T = Sophus::SE3d::exp(dx) * T;

    // ---- Iter statistics ----
    if (iter_stats) {
      IcpIterStats st;
      st.it = iter;
      st.N = static_cast<std::size_t>(used);
      st.inlier_ratio =
          static_cast<double>(used) /
          static_cast<double>(scan_body.size());

      st.mean_res = Mean(residuals);
      st.median_res = Median(residuals);
      st.mad_res = MAD(residuals, st.median_res);
      st.cond_JTJ = cond_JTJ;
      st.grad_norm = b.norm();

      st.yaw_raw = yaw_raw;
      st.yaw_clamped = yaw_clamped;
      st.yaw_clamp_triggered = yaw_clamp_triggered ? 1 : 0;
      st.yaw_frozen = yaw_frozen ? 1 : 0;
      st.eig_min = eig_min;
      st.eig_max = eig_max;
      st.eig_rank = eig_rank;
      st.yaw_info = yaw_info;
      st.yaw_freeze_reason = yaw_freeze_reason;
      st.cond_JTJ_scaled = cond_JTJ;
      st.used_corr_total = static_cast<std::size_t>(used);
      if (rp_.use_corr_budget && max_corr > 0) {
        st.budget_edge = budget_edge;
        st.budget_wall = budget_wall;
        st.budget_fallback_eff =
            std::max(0, max_corr - static_cast<int>(edge_used) - static_cast<int>(wall_used));
        st.corr_edge_used = edge_used;
        st.corr_wall_used = wall_used;
        st.corr_fallback_used = fallback_used;
        st.corr_build_early_stop = corr_build_early_stop;
        st.corr_budget_downgrade = corr_budget_downgrade;
        st.ring_query_downgrade = ring_query_downgrade;
      } else {
        st.budget_edge = 0;
        st.budget_wall = 0;
        st.budget_fallback_eff = used;
        st.corr_edge_used = 0;
        st.corr_wall_used = 0;
        st.corr_fallback_used = used;
        st.corr_build_early_stop = 0;
        st.corr_budget_downgrade = 0;
        st.ring_query_downgrade = 0;
      }
      st.corr_queried_points = attempts;

      st.alpha_p2plane = alpha;
      st.plane_used_ratio =
          (used > 0) ? (static_cast<double>(plane_used) / static_cast<double>(used)) : 0.0;
      st.plane_res_mean =
          (plane_used > 0) ? (plane_abs_sum / static_cast<double>(plane_used)) : 0.0;
      if (rp_.use_edge_first) {
        st.corr_edge_ratio =
            (used > 0) ? (static_cast<double>(edge_used) / static_cast<double>(used)) : 0.0;
        st.corr_wall_ratio =
            (used > 0) ? (static_cast<double>(wall_used) / static_cast<double>(used)) : 0.0;
        st.corr_fallback_ratio =
            (used > 0) ? (static_cast<double>(fallback_used) / static_cast<double>(used)) : 0.0;
        st.tau_edge = rp_.tau_edge;
        st.edge_first_used = 1;
        st.corr_highscore_ratio = st.corr_wall_ratio;
        st.highscore_fallback_ratio =
            (attempts > 0) ? (static_cast<double>(fallback_used) / static_cast<double>(attempts))
                           : 0.0;
      } else {
        st.corr_highscore_ratio =
            (used > 0) ? (static_cast<double>(highscore_used) / static_cast<double>(used)) : 0.0;
        st.highscore_fallback_ratio =
            (attempts > 0) ? (static_cast<double>(fallback_used) / static_cast<double>(attempts))
                           : 0.0;
        st.edge_first_used = 0;
      }
      st.score_tau_wall = rp_.score_tau_wall;
      st.score_filter_used = rp_.use_score_filter ? 1 : 0;

      if (!weights.empty()) {
        double sum_w = 0.0;
        double min_w = weights[0];
        double max_w = weights[0];
        for (double w : weights) {
          sum_w += w;
          min_w = std::min(min_w, w);
          max_w = std::max(max_w, w);
        }
        st.w_mean = sum_w / static_cast<double>(weights.size());
        st.w_min = min_w;
        st.w_max = max_w;
        const std::size_t idx = static_cast<std::size_t>(
            std::max<std::size_t>(1, (weights.size() * 9 + 9) / 10)) - 1;
        std::vector<double> wtmp = weights;
        std::nth_element(wtmp.begin(), wtmp.begin() + idx, wtmp.end());
        st.w_p90 = wtmp[idx];
        st.w_highscore_ratio =
            (weights.size() > 0)
                ? (static_cast<double>(weight_boost_used) / static_cast<double>(weights.size()))
                : 0.0;
      }
      if (!used_scores.empty()) {
        double sum_s = 0.0;
        for (double s : used_scores) sum_s += s;
        st.score_mean_used = sum_s / static_cast<double>(used_scores.size());
      }
      st.cost_u = 0.0;
      st.rel_drop_u = 0.0;
      st.stop_by_cost_u = 0;

      iter_stats->push_back(st);
    }

    // early-stop: dx
    if (dx.norm() < rp_.eps_dx) break;

    // early-stop: relative cost drop
    if (iter > 0 && std::isfinite(cost_prev) && cost_prev > 0.0) {
      const double rel_drop = std::abs(cost_prev - cost_now) / cost_prev;
      if (rel_drop < rp_.eps_cost_rel) break;
    }

    cost_prev = cost_now;
  }

  T_wb_io = T;
  return true;
}

}  // namespace struct_icp
