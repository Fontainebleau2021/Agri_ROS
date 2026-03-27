#pragma once

#include <Eigen/Core>
#include <unordered_map>
#include <vector>

namespace struct_icp {

struct VoxelMapParams {
  double voxel_size = 0.5;     // map resolution
  int max_neighbor_ring = 1;   // search in (2r+1)^3 neighbor voxels, r=1 => 27 cells
  int eig_update_every_K = 10;
  int eig_min_points = 20;
  bool enable_surfel_stats = true;
  bool enable_score = true;
  double score_beta = 0.05;
  double score_sigma_e = 0.05;
  double score_tau_wall = 0.35;
  double score_tau_edge = 0.5;
};

class VoxelHashMap {
public:
  struct VoxelKey {
    int x{0}, y{0}, z{0};
    bool operator==(const VoxelKey& o) const noexcept { return x == o.x && y == o.y && z == o.z; }
  };

  explicit VoxelHashMap(const VoxelMapParams& p = {}) : p_(p) {}

  void Clear();

  // Insert world points (already transformed to world)
  void InsertPoints(const std::vector<Eigen::Vector3d>& pts_w);

  // Query nearest centroid in neighbor voxels around pw
  bool QueryNearest(const Eigen::Vector3d& pw, Eigen::Vector3d& centroid_out) const;
  bool QueryNearestInRing(const Eigen::Vector3d& pw,
                          int ring,
                          Eigen::Vector3d& centroid_out) const;
  bool QueryNearestWithRing(const Eigen::Vector3d& pw,
                            int ring,
                            Eigen::Vector3d& centroid_out) const;

  struct SurfelHit {
    Eigen::Vector3d mu = Eigen::Vector3d::Zero();
    Eigen::Vector3d normal = Eigen::Vector3d::Zero();
    bool normal_valid = false;
    double d2 = 0.0;
    double score = 0.0;
    bool score_valid = false;
    double edge_score = 0.0;
    bool edge_valid = false;
    bool passed_score_filter = false;
    bool used_fallback = false;
  };

  bool QueryNearestSurfel(const Eigen::Vector3d& pw, SurfelHit& out) const;
  bool QueryNearestSurfelWithRing(const Eigen::Vector3d& pw,
                                  int ring,
                                  SurfelHit& out) const;
  bool QueryNearestSurfelAroundKey(const VoxelKey& center_key,
                                   int ring,
                                   const Eigen::Vector3d& pw,
                                   SurfelHit& out) const;
  bool QueryNearestSurfelFiltered(const Eigen::Vector3d& pw,
                                  double tau_wall,
                                  bool allow_fallback,
                                  SurfelHit& out,
                                  bool* used_fallback,
                                  bool* passed_filter) const;
  bool QueryNearestSurfelFilteredWithRing(const Eigen::Vector3d& pw,
                                          double tau_wall,
                                          bool allow_fallback,
                                          int ring,
                                          SurfelHit& out,
                                          bool* used_fallback,
                                          bool* passed_filter) const;
  enum class MatchTier { EDGE = 0, WALL = 1, FALLBACK = 2 };
  bool QueryNearestHierarchical(const Eigen::Vector3d& pw,
                                double tau_edge,
                                double tau_wall,
                                bool allow_fallback,
                                SurfelHit& out,
                                MatchTier* tier_out) const;
  bool QueryNearestHierarchicalWithRings(const Eigen::Vector3d& pw,
                                         double tau_edge,
                                         double tau_wall,
                                         bool allow_fallback,
                                         int ring_edge,
                                         int ring_wall,
                                         int ring_fallback,
                                         SurfelHit& out,
                                         MatchTier* tier_out,
                                         int* ring_used) const;

  double voxel_size() const { return p_.voxel_size; }
  int max_neighbor_ring() const { return p_.max_neighbor_ring; }

  struct PerfCounters {
    double dt_query_ms = 0.0;
    double dt_insert_ms = 0.0;
    double dt_eig_ms = 0.0;
  };

  void SetPerfEnabled(bool enabled) { perf_enabled_ = enabled; }
  void ResetPerf();
  PerfCounters GetPerfAndReset();

  struct StatsSummary {
    std::size_t voxel_count = 0;
    double mean_N = 0.0;
    int p90_N = 0;
    std::size_t eig_updated_voxels = 0;
  };

  StatsSummary GetStatsSummaryAndReset();

  struct ScoreSummary {
    std::size_t voxel_count = 0;
    double score_valid_ratio = 0.0;
    double score_mean = 0.0;
    double score_p90 = 0.0;
    double score_p99 = 0.0;
    double high_score_ratio = 0.0;
    double planarity_mean = 0.0;
    double linearity_mean = 0.0;
    double verticality_mean = 0.0;
    double temp_var_mean = 0.0;
  };

  ScoreSummary GetScoreSummary() const;

  struct EdgeScoreSummary {
    std::size_t voxel_count = 0;
    double edge_valid_ratio = 0.0;
    double edge_score_mean = 0.0;
    double edge_score_p90 = 0.0;
    double edge_score_p99 = 0.0;
    double high_edge_ratio = 0.0;
  };

  EdgeScoreSummary GetEdgeScoreSummary(double tau_edge) const;

  std::size_t VoxelCount() const { return map_.size(); }
  VoxelKey KeyFromPoint(const Eigen::Vector3d& p) const { return ToKey(p); }

private:
  struct VoxelKeyHash {
    std::size_t operator()(const VoxelKey& k) const noexcept {
      std::size_t h1 = std::hash<int>{}(k.x);
      std::size_t h2 = std::hash<int>{}(k.y);
      std::size_t h3 = std::hash<int>{}(k.z);
      return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
  };

  struct SurfelVoxel {
    int N = 0;
    Eigen::Vector3d mu = Eigen::Vector3d::Zero();
    Eigen::Matrix3d M2 = Eigen::Matrix3d::Zero();
    double t_last = 0.0;
    Eigen::Vector3d normal = Eigen::Vector3d::Zero();
    Eigen::Vector3d lambdas = Eigen::Vector3d::Zero();
    bool normal_valid = false;
    Eigen::Vector3d line_dir = Eigen::Vector3d::Zero();
    double edge_score = 0.0;
    bool edge_valid = false;
    double planarity = 0.0;
    double linearity = 0.0;
    double verticality = 0.0;
    double temp_var = 0.0;
    double temp_score = 0.0;
    double score = 0.0;
    bool score_valid = false;
    double m_e = 0.0;
    double v_e = 0.0;

    bool Add(const Eigen::Vector3d& p,
             double t,
             int eig_update_every_K,
             int eig_min_points,
             bool enable_surfel_stats,
             bool enable_score,
             double score_beta,
             double score_sigma_e,
             double* eig_ms_accum);
  };

  VoxelMapParams p_;
  std::unordered_map<VoxelKey, SurfelVoxel, VoxelKeyHash> map_;
  std::size_t eig_updates_since_last_ = 0;
  std::size_t time_counter_ = 0;
  bool perf_enabled_ = false;
  mutable PerfCounters perf_;

  VoxelKey ToKey(const Eigen::Vector3d& p) const;
};

}  // namespace struct_icp
