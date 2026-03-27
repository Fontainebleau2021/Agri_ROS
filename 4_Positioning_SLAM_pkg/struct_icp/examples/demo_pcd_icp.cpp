#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <sophus/se3.hpp>

#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>

#include "Preprocessing.hpp"
#include "Registration.hpp"
#include "CsvLogger.hpp"

static double NowSec() {
  using clock = std::chrono::steady_clock;
  return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

static std::vector<struct_icp::PointXYZI> LoadPCD_XYZI(const std::string& path) {
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

static std::vector<Eigen::Vector3d> ToEigenPoints(const std::vector<struct_icp::PointXYZI>& pts) {
  std::vector<Eigen::Vector3d> out;
  out.reserve(pts.size());
  for (const auto& p : pts) out.push_back(p.p);
  return out;
}

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage:\n"
              << "  demo_pcd_icp <pcd_ref> <pcd_cur> [out_dir]\n\n"
              << "Example:\n"
              << "  demo_pcd_icp ref.pcd cur.pcd ./out\n";
    return 1;
  }

  const std::string pcd_ref = argv[1];
  const std::string pcd_cur = argv[2];
  const std::string out_dir = (argc >= 4) ? argv[3] : ".";

  // 1) Load PCD
  auto ref_raw = LoadPCD_XYZI(pcd_ref);
  auto cur_raw = LoadPCD_XYZI(pcd_cur);

  // 2) Preprocess
  struct_icp::PreprocessParams pp;
  pp.min_range = 0.3;
  pp.max_range = 80.0;
  pp.enable_voxel_downsample = true;
  pp.voxel_size = 0.15;
  pp.decimate = 1;

  struct_icp::Preprocessing pre(pp);

  std::vector<struct_icp::PointXYZI> ref_ds, cur_ds;
  pre.Process(ref_raw, ref_ds);
  pre.Process(cur_raw, cur_ds);

  std::cout << "[demo] ref_ds=" << ref_ds.size() << " cur_ds=" << cur_ds.size() << "\n";

  // 3) Build voxel map from reference (world = ref frame)
  struct_icp::VoxelMapParams mp;
  mp.voxel_size = 0.5;
  mp.max_neighbor_ring = 1;

  struct_icp::RegistrationParams rp;
  rp.max_iters = 20;
  rp.eps_dx = 1e-4;
  rp.max_corr_dist = 1.0;
  rp.min_effective_corr = 50;

  struct_icp::Registration reg(rp, mp);
  reg.topk_K = 50;

  reg.SetMapFromPointsWorld(ToEigenPoints(ref_ds));

  // 4) Align current scan to map
  Sophus::SE3d T_wb = Sophus::SE3d();  // initial guess = Identity

  std::vector<struct_icp::IcpIterStats> iter_stats;
  std::vector<struct_icp::IcpTopKStats> topk_stats;

  const bool ok = reg.AlignPointToVoxelMap(cur_ds, T_wb, &iter_stats, &topk_stats);

  std::cout << "[demo] ok=" << ok
            << " iters=" << iter_stats.size()
            << " topk_batches=" << topk_stats.size() << "\n";
  std::cout << "[demo] T:\n" << T_wb.matrix() << "\n";

  // 5) Write CSV (optional)
  struct_icp::CsvLogger::Options opt;
  opt.diag_csv_path = out_dir + "/icp_diag.csv";
  opt.topk_csv_path = out_dir + "/icp_topk.csv";
  opt.flush_each_write = false;
  opt.precision = 8;

  struct_icp::CsvLogger logger(opt);

  const double t0 = NowSec();
  logger.ResetT0(t0);

  // 为了让 t_rel 不全是 0，这里用 “每迭代 1ms” 的虚拟时间步
  for (const auto& st : iter_stats) {
    logger.LogIcpIterStats(t0 + 1e-3 * double(st.it), st);
  }
  for (const auto& tk : topk_stats) {
    logger.LogTopKStats(t0 + 1e-3 * double(tk.it), tk);
  }

  std::cout << "[demo] wrote:\n"
            << "  " << opt.diag_csv_path << "\n"
            << "  " << opt.topk_csv_path << "\n";

  return ok ? 0 : 2;
}
