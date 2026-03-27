#pragma once

#include <Eigen/Core>
#include <cstddef>
#include <vector>
#include <array>

namespace struct_icp {

struct PointXYZI {
  Eigen::Vector3d p{0, 0, 0};
  double intensity{0.0};
};

// 预处理参数
struct PreprocessParams {
  // range filter
  double min_range = 0.0;
  double max_range = 200.0;

  // decimate: keep 1 every N points (N<=1 means keep all)
  int decimate = 1;

  // legacy single-output downsample
  bool enable_voxel_downsample = true;
  double voxel_size = 0.15;

  // split-output mode
  bool enable_split_output = false;

  // icp points
  bool icp_enable_voxel = true;
  double icp_voxel_size = 0.15;
  std::size_t max_icp_points{5000};  // budget

  // map points
  bool map_enable_voxel = true;
  double map_voxel_size = 0.15;
};

struct PreprocessOutput {
  std::vector<PointXYZI> icp_points;
  std::vector<PointXYZI> map_points;

  // --- diagnostics ---
  std::size_t n_raw   = 0;
  std::size_t n_split = 0;
  std::array<std::size_t,3> icp_r_bins{{0,0,0}};
  std::array<std::size_t,3> map_r_bins{{0,0,0}};
};

class Preprocessing {
public:
  explicit Preprocessing(const PreprocessParams& p = {}) : p_(p) {}

  // legacy: output a single point cloud
  void Process(const std::vector<PointXYZI>& in,
               std::vector<PointXYZI>& out) const;

  // split: output icp_points + map_points
  void ProcessSplit(const std::vector<PointXYZI>& in,
                    std::vector<PointXYZI>& icp_points,
                    std::vector<PointXYZI>& map_points) const;

  // convenience overload for pipeline (so you can call ProcessSplit(in, po))
  void ProcessSplit(const std::vector<PointXYZI>& in,
                    PreprocessOutput& out) const;

private:
  PreprocessParams p_;
};

}  // namespace struct_icp
