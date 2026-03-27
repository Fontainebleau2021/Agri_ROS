#include "Preprocessing.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace struct_icp {
namespace {

// ---------------- helpers ----------------
static inline bool IsFinite(const PointXYZI& p) {
  return std::isfinite(p.p.x()) && std::isfinite(p.p.y()) && std::isfinite(p.p.z());
}

static inline double Range(const PointXYZI& p) {
  return p.p.norm();
}

struct VoxelKey {
  int x{0}, y{0}, z{0};
  bool operator==(const VoxelKey& o) const { return x == o.x && y == o.y && z == o.z; }
};

struct VoxelKeyHash {
  std::size_t operator()(const VoxelKey& k) const noexcept {
    // simple hash combine
    const std::size_t hx = std::hash<int>{}(k.x);
    const std::size_t hy = std::hash<int>{}(k.y);
    const std::size_t hz = std::hash<int>{}(k.z);
    return hx * 1315423911u ^ (hy << 1) ^ (hz << 7);
  }
};

static inline VoxelKey MakeKey(const Eigen::Vector3d& p, double vs) {
  const double inv = 1.0 / vs;
  return VoxelKey{
      static_cast<int>(std::floor(p.x() * inv)),
      static_cast<int>(std::floor(p.y() * inv)),
      static_cast<int>(std::floor(p.z() * inv))};
}

// 体素均值下采样：每个 voxel 求均值点（与KISS-ICP那种“均值点”一致风格）
static void VoxelDownsampleMean(const std::vector<PointXYZI>& in,
                                double voxel_size,
                                std::vector<PointXYZI>& out) {
  out.clear();
  if (in.empty()) return;
  if (!(voxel_size > 0.0)) { out = in; return; }

  struct Accum {
    Eigen::Vector3d sum_p{0,0,0};
    double sum_i{0.0};
    int n{0};
  };

  std::unordered_map<VoxelKey, Accum, VoxelKeyHash> map;
  map.reserve(in.size() / 2 + 1);

  for (const auto& pt : in) {
    const VoxelKey k = MakeKey(pt.p, voxel_size);
    auto& a = map[k];
    a.sum_p += pt.p;
    a.sum_i += pt.intensity;
    a.n += 1;
  }

  out.reserve(map.size());
  for (const auto& kv : map) {
    const auto& a = kv.second;
    PointXYZI p;
    p.p = a.sum_p / static_cast<double>(a.n);
    p.intensity = a.sum_i / static_cast<double>(a.n);
    out.push_back(p);
  }
}

// 结构化限点：先按方位角分桶，再在桶内等步长抽样（保证空间覆盖）
static void LimitToN_Structured(std::vector<PointXYZI>& pts,
                                std::size_t max_n,
                                int angle_bins = 360) {
  if (max_n == 0 || pts.empty()) { pts.clear(); return; }
  if (pts.size() <= max_n) return;

  angle_bins = std::max(16, angle_bins);

  std::vector<std::vector<PointXYZI>> buckets(static_cast<std::size_t>(angle_bins));
  buckets.shrink_to_fit();
  buckets.resize(static_cast<std::size_t>(angle_bins));

  for (auto& p : pts) {
    const double ang = std::atan2(p.p.y(), p.p.x());              // [-pi,pi]
    const double u = (ang + M_PI) * (static_cast<double>(angle_bins) / (2.0 * M_PI));
    int b = static_cast<int>(std::floor(u));
    if (b < 0) b = 0;
    if (b >= angle_bins) b = angle_bins - 1;
    buckets[static_cast<std::size_t>(b)].push_back(p);
  }

  // 每个桶先按距离排序（近的优先），再抽样
  for (auto& bk : buckets) {
    std::sort(bk.begin(), bk.end(), [](const PointXYZI& a, const PointXYZI& b) {
      return a.p.squaredNorm() < b.p.squaredNorm();
    });
  }

  std::vector<PointXYZI> out;
  out.reserve(max_n);

  // 先保证每个桶最多拿 quota 个
  const std::size_t bins = buckets.size();
  const std::size_t quota = std::max<std::size_t>(1, max_n / std::max<std::size_t>(1, bins));

  for (auto& bk : buckets) {
    if (out.size() >= max_n) break;
    const std::size_t take = std::min<std::size_t>(quota, bk.size());
    for (std::size_t i = 0; i < take && out.size() < max_n; ++i) out.push_back(bk[i]);
  }

  // 如果还不够，再 round-robin 补齐
  std::size_t idx = quota;
  while (out.size() < max_n) {
    bool added = false;
    for (auto& bk : buckets) {
      if (out.size() >= max_n) break;
      if (idx < bk.size()) {
        out.push_back(bk[idx]);
        added = true;
      }
    }
    if (!added) break;
    ++idx;
  }

  pts.swap(out);
}

// 过滤 + decimate
static void FilterAndDecimate(const std::vector<PointXYZI>& in,
                              double min_r, double max_r,
                              int decimate,
                              std::vector<PointXYZI>& out) {
  out.clear();
  if (in.empty()) return;

  decimate = std::max(1, decimate);
  out.reserve(in.size() / static_cast<std::size_t>(decimate) + 1);

  std::size_t keep_i = 0;
  for (std::size_t i = 0; i < in.size(); ++i) {
    if ((i % static_cast<std::size_t>(decimate)) != 0) continue;

    const auto& p = in[i];
    if (!IsFinite(p)) continue;
    const double r = Range(p);
    if (r < min_r || r > max_r) continue;

    out.push_back(p);
    (void)keep_i;
  }
}

}  // namespace

// ---------------- Preprocessing ----------------
void Preprocessing::Process(const std::vector<PointXYZI>& in,
                            std::vector<PointXYZI>& out) const {
  std::vector<PointXYZI> tmp;
  FilterAndDecimate(in, p_.min_range, p_.max_range, p_.decimate, tmp);

  if (p_.enable_voxel_downsample && p_.voxel_size > 0.0) {
    VoxelDownsampleMean(tmp, p_.voxel_size, out);
  } else {
    out.swap(tmp);
  }
}

void Preprocessing::ProcessSplit(const std::vector<PointXYZI>& in,
                                 std::vector<PointXYZI>& icp_points,
                                 std::vector<PointXYZI>& map_points) const {
  // 若不开 split，则兼容老逻辑：两份都给同一份（或只给 icp）
  if (!p_.enable_split_output) {
    Process(in, icp_points);
    map_points = icp_points;
    return;
  }

  // 共同的过滤+抽稀
  std::vector<PointXYZI> tmp;
  FilterAndDecimate(in, p_.min_range, p_.max_range, p_.decimate, tmp);

  // map_points：更保真（通常点更多）
  if (p_.map_enable_voxel && p_.map_voxel_size > 0.0) {
    VoxelDownsampleMean(tmp, p_.map_voxel_size, map_points);
  } else {
    map_points = tmp;
  }

  // icp_points：更稀疏 + 限预算（结构化）
  if (p_.icp_enable_voxel && p_.icp_voxel_size > 0.0) {
    VoxelDownsampleMean(tmp, p_.icp_voxel_size, icp_points);
  } else {
    icp_points = tmp;
  }

  // 结构化限点（避免随机截断破坏覆盖）
  LimitToN_Structured(icp_points, p_.max_icp_points);
}

void Preprocessing::ProcessSplit(const std::vector<PointXYZI>& in, PreprocessOutput& out) const {
  out.n_raw = in.size();
  std::vector<PointXYZI> icp_points;
  std::vector<PointXYZI> map_points;
  ProcessSplit(in, icp_points, map_points);
  out.icp_points.swap(icp_points);
  out.map_points.swap(map_points);

  // split/滤波后的临时点数：这里无法直接拿到 tmp 的 size（3-arg 版本内部临时变量），
  // 先用 out.map_points 近似表示“用于建图的输出点数”。
  out.n_split = out.map_points.size();

  auto bin3 = [](const std::vector<PointXYZI>& pts) {
    std::array<std::size_t,3> b{{0,0,0}};
    constexpr double r0 = 5.0;
    constexpr double r1 = 15.0;
    for (const auto& p : pts) {
      const double r = std::sqrt(p.p.x()*p.p.x() + p.p.y()*p.p.y() + p.p.z()*p.p.z());
      if (r < r0) b[0]++; else if (r < r1) b[1]++; else b[2]++;
    }
    return b;
  };
  out.icp_r_bins = bin3(out.icp_points);
  out.map_r_bins = bin3(out.map_points);
}

}  // namespace struct_icp
