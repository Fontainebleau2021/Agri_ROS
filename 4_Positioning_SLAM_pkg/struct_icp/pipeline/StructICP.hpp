#pragma once

#include <Eigen/Core>
#include <memory>
#include <string>
#include <vector>

#include <sophus/se3.hpp>

#include "CsvLogger.hpp"
#include "LocalMap.hpp"
#include "Preprocessing.hpp"
#include "Registration.hpp"

namespace struct_icp::pipeline {

struct PipelineParams {
  // I/O
  std::string input_dir;
  std::string output_dir;
  std::string pcd_ext = ".pcd";

  // range
  int start_frame = 0;
  int max_frames = -1;  // -1: all

  // dt for trajectory timestamps (index*dt)
  double frame_dt = 0.1;

  // initial pose
  Sophus::SE3d T_wb0 = Sophus::SE3d();

  // map insert policy
  bool insert_every_frame = true;
  int insert_every_n = 1;

  // logging / outputs
  bool log_csv = true;
  bool write_traj_tum = true;
  bool verbose = true;
  bool enable_perf = false;
  bool debug_map_ready = false;

  // top-k debug
  int topk_K = 50;

  // ---- Local sliding-window map ----
  int local_window_size = 30;           // keep last K inserted frames
  bool local_use_radius_crop = false;   // optional cropping around current pose
  double local_crop_radius = 25.0;      // meters
  int local_rebuild_every_n = 5;        // rebuild every n inserts (1 = every insert)
  bool enable_async_rebuild = false;
  double rebuild_min_move = 0.0;
  int async_warmup_frames = 1;
  bool async_wait_first_map = true;
  int async_wait_timeout_ms = 200;
  int min_voxels_for_icp = 50;
  bool enable_adaptive_ring = false;
  double tau_fallback_hi = 0.6;
  double tau_fallback_lo = 0.3;
  int fallback_hi_frames = 3;
  int fallback_lo_frames = 5;
  int degrade_hold_frames = 10;
  int adaptive_ring_warmup = 5;
  bool enable_assoc_map = false;
  double assoc_voxel = 1.0;
  int assoc_ring_fallback = 1;
  bool assoc_refine_with_fine = false;
  int assoc_refine_ring = 1;
  int assoc_min_voxels_for_icp = 50;
  bool enable_coarse_assoc = false;
  double coarse_voxel_mul = 2.0;
  int coarse_ring = 0;
  int coarse_refine_ring = 0;
  bool coarse_fallback_full = true;
  int min_coarse_voxels_for_icp = 50;
  bool enable_adaptive_refine_ring = false;
  int refine_ring_min = 0;
  int refine_ring_max = 1;
  double fallback_ratio_hi = 0.25;
  double fallback_ratio_lo = 0.15;
  double coarse_hit_ratio_lo = 0.60;
  int adapt_window = 5;
  int adapt_warmup_frames = 3;
};

class StructICP {
public:
  StructICP(const struct_icp::PreprocessParams& pp,
            const struct_icp::RegistrationParams& rp,
            const struct_icp::VoxelMapParams& mp,
            const PipelineParams& pip);

  bool RunPcdDirectory();

private:
  struct AdaptiveRingState {
    bool degraded = false;
    int hi_count = 0;
    int lo_count = 0;
    int hold_left = 0;
  };
  static double NowSec();
  static bool EnsureDirExists(const std::string& dir);
  static std::vector<std::string> ListFilesSorted(const std::string& dir,
                                                  const std::string& ext);

  static std::vector<struct_icp::PointXYZI> LoadPCD_XYZI(const std::string& path);

  static std::vector<Eigen::Vector3d> TransformToWorld(
      const std::vector<struct_icp::PointXYZI>& pts_body,
      const Sophus::SE3d& T_wb);

  static void WriteTumLine(std::ofstream& ofs, double t, const Sophus::SE3d& T);

private:
  // params
  struct_icp::PreprocessParams pp_;
  struct_icp::RegistrationParams rp_;
  struct_icp::VoxelMapParams mp_;
  PipelineParams pip_;

  // modules
  struct_icp::Preprocessing pre_;
  struct_icp::Registration reg_;

  // local sliding-window map
  struct_icp::LocalMap local_map_;
  AdaptiveRingState adaptive_ring_;
  std::deque<double> coarse_fb_hist_;
  std::deque<double> coarse_hit_hist_;
  int refine_ring_eff_ = 0;

  // CsvLogger options (NOTE: use CsvLogger::Options)
  struct_icp::CsvLogger::Options log_opt_;
};

}  // namespace struct_icp::pipeline
