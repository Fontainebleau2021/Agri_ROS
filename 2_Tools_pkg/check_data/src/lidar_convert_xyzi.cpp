#include <ros/ros.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/point_cloud2_iterator.h>   // 迭代器
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <limits>
#include <cmath>
#include "check_data/color.h"

ros::Publisher pub_clean;
bool debug = true;

// ——把 ROS PointField 的 datatype 映射成人类可读字符串
static inline const char* dtypeStr(uint8_t dt) {
  using PF = sensor_msgs::PointField;
  switch (dt) {
    case PF::INT8:    return "INT8";
    case PF::UINT8:   return "UINT8";
    case PF::INT16:   return "INT16";
    case PF::UINT16:  return "UINT16";
    case PF::INT32:   return "INT32";
    case PF::UINT32:  return "UINT32";
    case PF::FLOAT32: return "FLOAT32";
    case PF::FLOAT64: return "FLOAT64";
    default:          return "UNKNOWN";
  }
}

static inline size_t dtypeSize(uint8_t dt) {
  using PF = sensor_msgs::PointField;
  switch (dt) {
    case PF::INT8: case PF::UINT8:   return 1;
    case PF::INT16: case PF::UINT16: return 2;
    case PF::INT32: case PF::UINT32:
    case PF::FLOAT32:                return 4;
    case PF::FLOAT64:                return 8;
    default:                         return 0;
  }
}

// ——打印一行简明的点云格式摘要
static inline void describePointCloud2(const sensor_msgs::PointCloud2& msg) {
  const bool organized = (msg.height > 1);
  const size_t pts = static_cast<size_t>(msg.width) * static_cast<size_t>(msg.height);
  // 统计常见字段是否存在以及 intensity 的具体类型
  auto findFieldIndex = [](const sensor_msgs::PointCloud2& m, const std::string& n)->int{
    for (size_t i=0;i<m.fields.size();++i) if (m.fields[i].name==n) return static_cast<int>(i);
    return -1;
  };
  const int i_idx = findFieldIndex(msg,"intensity");
  const int r_idx = findFieldIndex(msg,"ring");
  const int t_idx = findFieldIndex(msg,"time"); // 有些雷达用 "t"/"timestamp"，可按需再加

  // 计算 fields 的“最小可容纳 point_step”（考虑 offset+size*count 的最大端点）
  size_t min_point_step = 0;
  for (const auto& f : msg.fields) {
    size_t end = static_cast<size_t>(f.offset) + dtypeSize(f.datatype) * std::max<uint32_t>(1, f.count);
    if (end > min_point_step) min_point_step = end;
  }

  std::ostringstream fss;
  for (size_t i=0;i<msg.fields.size();++i) {
    const auto& f = msg.fields[i];
    if (i) fss << ", ";
    fss << f.name << "(" << dtypeStr(f.datatype) << "x" << std::max<uint32_t>(1,f.count)
        << " @" << f.offset << ")";
  }

  if (debug) 
  {
    ROS_INFO_STREAM_THROTTLE(1.0,
    "[schema] " << (organized ? "organized" : "unorganized")
    << " (" << msg.width << "x" << msg.height << ", pts=" << pts << ") "
    << "frame=" << msg.header.frame_id
    << " is_dense=" << (msg.is_dense ? "true" : "false")
    << " endian=" << (msg.is_bigendian ? "BE" : "LE")
    << " point_step=" << msg.point_step
    << " row_step=" << msg.row_step
      << " min_point_step_by_fields=" << min_point_step
    );

    ROS_INFO_STREAM_THROTTLE(1.0,
      "[fields] " << fss.str()
    );

    ROS_INFO_STREAM_THROTTLE(1.0,
      "[flags] intensity=" << (i_idx>=0 ? dtypeStr(msg.fields[i_idx].datatype) : "none")
      << ", ring=" << (r_idx>=0 ? "yes" : "no")
      << ", time=" << (t_idx>=0 ? "yes" : "no")
    );

  }
  

  if (msg.point_step < min_point_step) {
    ROS_WARN_THROTTLE(2.0, "point_step(%u) < min_point_step_by_fields(%zu): 字段定义和步长可能不一致或存在填充问题。",
                      msg.point_step, min_point_step);
  }
}

static inline int findFieldIndex(const sensor_msgs::PointCloud2& msg, const std::string& name) {
  for (size_t i = 0; i < msg.fields.size(); ++i) {
    if (msg.fields[i].name == name) return static_cast<int>(i);
  }
  return -1;
}

void cb(const sensor_msgs::PointCloud2ConstPtr& msg)
{
  describePointCloud2(*msg);
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

  // 2) 生成“有效点”索引（NaN/Inf 过滤）
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

  // 3) 判定是否有 intensity 字段，以及其数据类型
  const int i_idx = findFieldIndex(*msg, "intensity");
  const bool has_intensity = (i_idx >= 0);
  uint8_t intensity_dtype = 0;
  if (has_intensity) intensity_dtype = msg->fields[i_idx].datatype;

  // 4) 构造仅含 xyz 或 xyzi 的输出消息（高度设为 1，打平为非组织化点云）
  sensor_msgs::PointCloud2 out;
  out.header = msg->header;
  out.is_dense = true; // 我们只写入有限点
  out.height = 1;
  out.width  = keep_indices.size();

  sensor_msgs::PointCloud2Modifier mod(out);
  if (has_intensity) {
    mod.setPointCloud2Fields(4,
      "x", 7, 1,   // FLOAT32
      "y", 7, 1,
      "z", 7, 1,
      "intensity", 7, 1  // 输出统一为 FLOAT32
    );
  } else {
    mod.setPointCloud2Fields(3,
      "x", 7, 1,
      "y", 7, 1,
      "z", 7, 1
    );
  }
  mod.resize(keep_indices.size());

  // 5) 迭代写出（只拷贝 keep 的点；若有 intensity，按原类型转为 float32）
  // 输入迭代器（x,y,z 固定为 float32）
  sensor_msgs::PointCloud2ConstIterator<float> ix(*msg, "x");
  sensor_msgs::PointCloud2ConstIterator<float> iy(*msg, "y");
  sensor_msgs::PointCloud2ConstIterator<float> iz(*msg, "z");

  // 输出迭代器
  sensor_msgs::PointCloud2Iterator<float> ox(out, "x");
  sensor_msgs::PointCloud2Iterator<float> oy(out, "y");
  sensor_msgs::PointCloud2Iterator<float> oz(out, "z");
  std::unique_ptr<sensor_msgs::PointCloud2Iterator<float>> oi; // 可空
  if (has_intensity) oi.reset(new sensor_msgs::PointCloud2Iterator<float>(out, "intensity"));

  // 准备一个“期望保留”的索引游标，线性扫描避免查表
  size_t k = 0;
  const size_t K = keep_indices.size();
  int idx = 0;

  // 针对不同 intensity 数据类型分别建立输入迭代器（如果有）
  // 并在统一循环里做写入（只在 idx 命中 keep_indices[k] 时写）
  // intensity 将转换为 float 存入输出
  switch (has_intensity ? intensity_dtype : 0) {
    case 0: { // 无 intensity，纯 xyz
      for (; ix != ix.end(); ++ix, ++iy, ++iz, ++idx) {
        if (k < K && idx == keep_indices[k]) {
          *ox = *ix; *oy = *iy; *oz = *iz;
          ++ox; ++oy; ++oz;
          ++k;
        }
      }
    } break;
    case sensor_msgs::PointField::FLOAT32: {
      sensor_msgs::PointCloud2ConstIterator<float> ii(*msg, "intensity");
      for (; ix != ix.end(); ++ix, ++iy, ++iz, ++ii, ++idx) {
        if (k < K && idx == keep_indices[k]) {
          *ox = *ix; *oy = *iy; *oz = *iz; **oi = *ii;
          ++ox; ++oy; ++oz; ++(*oi);
          ++k;
        }
      }
    } break;
    case sensor_msgs::PointField::UINT16: {
      sensor_msgs::PointCloud2ConstIterator<uint16_t> ii(*msg, "intensity");
      for (; ix != ix.end(); ++ix, ++iy, ++iz, ++ii, ++idx) {
        if (k < K && idx == keep_indices[k]) {
          *ox = *ix; *oy = *iy; *oz = *iz; **oi = static_cast<float>(*ii);
          ++ox; ++oy; ++oz; ++(*oi);
          ++k;
        }
      }
    } break;
    case sensor_msgs::PointField::INT16: {
      sensor_msgs::PointCloud2ConstIterator<int16_t> ii(*msg, "intensity");
      for (; ix != ix.end(); ++ix, ++iy, ++iz, ++ii, ++idx) {
        if (k < K && idx == keep_indices[k]) {
          *ox = *ix; *oy = *iy; *oz = *iz; **oi = static_cast<float>(*ii);
          ++ox; ++oy; ++oz; ++(*oi);
          ++k;
        }
      }
    } break;
    case sensor_msgs::PointField::UINT8: {
      sensor_msgs::PointCloud2ConstIterator<uint8_t> ii(*msg, "intensity");
      for (; ix != ix.end(); ++ix, ++iy, ++iz, ++ii, ++idx) {
        if (k < K && idx == keep_indices[k]) {
          *ox = *ix; *oy = *iy; *oz = *iz; **oi = static_cast<float>(*ii);
          ++ox; ++oy; ++oz; ++(*oi);
          ++k;
        }
      }
    } break;
    case sensor_msgs::PointField::INT8: {
      sensor_msgs::PointCloud2ConstIterator<int8_t> ii(*msg, "intensity");
      for (; ix != ix.end(); ++ix, ++iy, ++iz, ++ii, ++idx) {
        if (k < K && idx == keep_indices[k]) {
          *ox = *ix; *oy = *iy; *oz = *iz; **oi = static_cast<float>(*ii);
          ++ox; ++oy; ++oz; ++(*oi);
          ++k;
        }
      }
    } break;
    default: {
      // 其他非常见类型，做一次警告并按无 intensity 处理
      ROS_WARN_THROTTLE(2.0, "Unsupported intensity datatype=%u, exporting XYZ only.", (unsigned)intensity_dtype);
      for (; ix != ix.end(); ++ix, ++iy, ++iz, ++idx) {
        if (k < K && idx == keep_indices[k]) {
          *ox = *ix; *oy = *iy; *oz = *iz;
          ++ox; ++oy; ++oz;
          ++k;
        }
      }
    } break;
  }

  pub_clean.publish(out);
}

int main(int argc, char** argv)
{
  ros::init(argc, argv, "lidar_convert_xyzi");
  ros::NodeHandle nh_;
  std::string lidar_topic_;
  std::string lidar_clean_topic_;
  nh_.param<std::string>("lidar_topic", lidar_topic_, "/ouster/points");
  nh_.param<std::string>("lidar_clean_topic", lidar_clean_topic_, "/ouster/points_convert");
  nh_.param<bool>("debug_info", debug, true);
  std::cout << BOLDGREEN << "[orign]: " << BOLDYELLOW << lidar_topic_ << RESET <<std::endl;
  std::cout << BOLDGREEN << "[clean]: " << BOLDYELLOW << lidar_clean_topic_ << RESET <<std::endl; 
  std::cout << BOLDGREEN << "[debug]: " << BOLDYELLOW << debug << RESET <<std::endl;
  ros::Subscriber sub = nh_.subscribe<sensor_msgs::PointCloud2>(lidar_topic_, 1, cb);
  pub_clean = nh_.advertise<sensor_msgs::PointCloud2>(lidar_clean_topic_, 1);
  ros::spin();
  return 0;
}