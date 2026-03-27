#include <ros/ros.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/point_cloud2_iterator.h>   // 迭代器
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/filters/extract_indices.h>         // ExtractIndices on PCLPointCloud2
#include <limits>
#include <cmath>
#include "check_data/color.h"

ros::Publisher pub_clean;
bool debug = true;

void cb(const sensor_msgs::PointCloud2ConstPtr& msg)
{
  // 1) 统计信息（直接在 msg 上用迭代器，不改变数据）
  size_t total = msg->width * msg->height;
  size_t bad   = 0;
  double rmin  = std::numeric_limits<double>::infinity();
  double rmax  = 0.0;

  {
    sensor_msgs::PointCloud2ConstIterator<float> it_x(*msg, "x");
    sensor_msgs::PointCloud2ConstIterator<float> it_y(*msg, "y");
    sensor_msgs::PointCloud2ConstIterator<float> it_z(*msg, "z");
    for (; it_x != it_x.end(); ++it_x, ++it_y, ++it_z) {
      float x = *it_x, y = *it_y, z = *it_z;
      if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) { bad++; continue; }
      double r = std::sqrt(double(x)*x + double(y)*y + double(z)*z);
      if (std::isfinite(r)) {
        if (r < rmin) rmin = r;
        if (r > rmax) rmax = r;
      }
    }
  }
  double bad_ratio = total ? (double)bad / (double)total : 0.0;
  if (debug) 
  ROS_INFO_STREAM_THROTTLE(1.0,
    "points=" << total
    << " bad=" << bad << " (" << 100.0 * bad_ratio << "%)"
    << " r[min,max]=[" << rmin << "," << rmax << "]");

  // 2) 生成“有效点”索引（仍然基于原 msg）
  std::vector<int> keep_indices;
  keep_indices.reserve(total);
  {
    sensor_msgs::PointCloud2ConstIterator<float> it_x(*msg, "x");
    sensor_msgs::PointCloud2ConstIterator<float> it_y(*msg, "y");
    sensor_msgs::PointCloud2ConstIterator<float> it_z(*msg, "z");
    int idx = 0;
    for (; it_x != it_x.end(); ++it_x, ++it_y, ++it_z, ++idx) {
      float x = *it_x, y = *it_y, z = *it_z;
      if (std::isfinite(x) && std::isfinite(y) && std::isfinite(z)) {
        keep_indices.push_back(idx);
      }
    }
  }

  // 3) 把 ROS msg 转成 PCL 的“通用格式” PCLPointCloud2（不丢字段）
  pcl::PCLPointCloud2::Ptr cloud2(new pcl::PCLPointCloud2);
  pcl_conversions::toPCL(*msg, *cloud2);

  // 4) 在 PCLPointCloud2 上按索引抽取（保留所有原 fields）
  pcl::PCLPointCloud2::Ptr cloud2_kept(new pcl::PCLPointCloud2);
  {
    pcl::ExtractIndices<pcl::PCLPointCloud2> ex;
    ex.setInputCloud(cloud2);
    pcl::PointIndices::Ptr pi(new pcl::PointIndices);
    pi->indices.swap(keep_indices);
    ex.setIndices(pi);
    ex.setNegative(false); // 取“保留”的索引
    ex.filter(*cloud2_kept);
  }

  // 5) 回到 ROS 消息并发布（header 继承原始）
  sensor_msgs::PointCloud2 out;
  pcl_conversions::fromPCL(*cloud2_kept, out);
  out.header = msg->header;
  out.is_dense = true; // 清理后可标记为 dense
  pub_clean.publish(out);

  // ------ 可选的后续滤波（示例思路）------
  // 你如果想在 NaN 清理后继续做 SOR/半径滤波，又要“保留字段”，建议流程是：
  // a) 用迭代器得到 keep_indices（上面第 2 步）；
  // b) 同时把 msg 转成 PointXYZI（或你需要的点型）以便做几何滤波；
  // c) 在 PointXYZI 上用 setIndices(keep_indices) 做 SOR/ROR，得到第二批 kept 索引（相对于 a 中的子集索引）；
  // d) 把这批索引“映射回”原始全量索引（用 keep_indices[local_i]），得到 final_indices；
  // e) 最后仍然用 ExtractIndices<pcl::PCLPointCloud2> 在 cloud2 上抽取 final_indices。
}

int main(int argc, char** argv)
{
  ros::init(argc, argv, "lidar_checker");
  ros::NodeHandle nh_;
  std::string lidar_topic_;
  std::string lidar_clean_topic_;
  nh_.param<std::string>("lidar_topic", lidar_topic_, "/ouster/points");
  nh_.param<std::string>("lidar_clean_topic", lidar_clean_topic_, "/ouster/points_clean");
  nh_.param<bool>("debug_info", debug, true);
  std::cout << BOLDGREEN << "[orign]: " << BOLDYELLOW << lidar_topic_ << RESET <<std::endl;
  std::cout << BOLDGREEN << "[clean]: " << BOLDYELLOW << lidar_clean_topic_ << RESET <<std::endl; 
  std::cout << BOLDGREEN << "[debug]: " << BOLDYELLOW << debug << RESET <<std::endl;
  ros::Subscriber sub = nh_.subscribe<sensor_msgs::PointCloud2>(lidar_topic_, 1, cb);
  pub_clean = nh_.advertise<sensor_msgs::PointCloud2>(lidar_clean_topic_, 1);
  ros::spin();
  return 0;
}
