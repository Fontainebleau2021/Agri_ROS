#pragma once
#include <Eigen/Core>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace struct_icp {

// ---------- robust stats ----------
inline double Mean(const std::vector<double>& v) {
  if (v.empty()) return 0.0;
  double s = 0.0;
  for (double x : v) s += x;
  return s / static_cast<double>(v.size());
}

// Median that does NOT destroy input
inline double Median(std::vector<double> v) {
  if (v.empty()) return 0.0;
  const std::size_t n = v.size();
  const std::size_t mid = n / 2;
  std::nth_element(v.begin(), v.begin() + mid, v.end());
  double med = v[mid];
  if ((n % 2) == 0) {
    // need the lower middle
    auto it = std::max_element(v.begin(), v.begin() + mid);
    med = 0.5 * (med + *it);
  }
  return med;
}

inline double MAD(const std::vector<double>& v, double median) {
  if (v.empty()) return 0.0;
  std::vector<double> dev;
  dev.reserve(v.size());
  for (double x : v) dev.push_back(std::abs(x - median));
  return Median(std::move(dev));
}

// ---------- condition number ----------
inline double ConditionNumberJTJ(const Eigen::Matrix<double,6,6>& H) {
  // Gershgorin bounds proxy (avoid full eigen-decomposition).
  double lmin = std::numeric_limits<double>::infinity();
  double lmax = 0.0;
  for (int i = 0; i < 6; ++i) {
    double radius = 0.0;
    for (int j = 0; j < 6; ++j) {
      if (j == i) continue;
      radius += std::abs(H(i, j));
    }
    const double diag = std::abs(H(i, i));
    lmin = std::min(lmin, diag - radius);
    lmax = std::max(lmax, diag + radius);
  }

  const double eps = 1e-12;
  if (lmax < eps) return std::numeric_limits<double>::infinity();
  // Gershgorin lmin can be negative; denom=max(lmin, eps) flags ill-conditioning.
  const double denom = std::max(lmin, eps);
  return lmax / denom;
}

// ---------- yaw observability (Schur complement) ----------
inline double YawInfoSchur(const Eigen::Matrix<double,6,6>& H) {
  const Eigen::Matrix<double,3,3> H_tt = H.block<3,3>(0,0);
  const Eigen::Matrix<double,3,3> H_tw = H.block<3,3>(0,3);
  const Eigen::Matrix<double,3,3> H_wt = H.block<3,3>(3,0);
  const Eigen::Matrix<double,3,3> H_ww = H.block<3,3>(3,3);

  Eigen::LDLT<Eigen::Matrix<double,3,3>> ldlt(H_tt);
  if (ldlt.info() != Eigen::Success) return 0.0;

  const Eigen::Matrix<double,3,3> S = H_ww - H_wt * ldlt.solve(H_tw);
  return S(2,2);
}

// ---------- top-k helper ----------
struct TopKItem {
  int idx = -1;
  double key = 0.0; // sort by this descending
};

// Return indices of top-k by key (descending)
inline std::vector<int> TopKIndices(const std::vector<double>& key, int K) {
  std::vector<int> out;
  if (K <= 0 || key.empty()) return out;
  K = std::min<int>(K, static_cast<int>(key.size()));
  std::vector<TopKItem> items;
  items.reserve(key.size());
  for (int i = 0; i < (int)key.size(); ++i) items.push_back({i, key[i]});
  std::nth_element(items.begin(), items.begin() + (K - 1), items.end(),
                   [](const TopKItem& a, const TopKItem& b) { return a.key > b.key; });
  items.resize(K);
  std::sort(items.begin(), items.end(),
            [](const TopKItem& a, const TopKItem& b) { return a.key > b.key; });
  out.reserve(K);
  for (const auto& it : items) out.push_back(it.idx);
  return out;
}

} // namespace struct_icp
