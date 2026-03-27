#include <ros/ros.h>
#include <sensor_msgs/Imu.h>
#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/NavSatFix.h>

#include <string>
#include <deque>

#include <csignal>
#include <atomic>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <boost/filesystem.hpp>

#include <pcl_conversions/pcl_conversions.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>
#include <pcl/filters/filter.h>                  // removeNaNFromPointCloud
#include <pcl/filters/statistical_outlier_removal.h>
#include <pcl/filters/radius_outlier_removal.h>

#include <unordered_map>

#include "check_data/color.h"

#define RESULT_FILE_DIR(name)    (std::string(std::string(ROOT_DIR) + "result/"+ name))

bool flg_exit = false;
void SigHandle(int sig) {
    flg_exit = true;
}

struct FieldInfo {
  uint32_t offset;   // 字节偏移
  uint8_t  datatype; // sensor_msgs::PointField::FLOAT32 等
  uint32_t count;    // 元素个数（数组时>1）
};

static inline const char* typeStr(uint8_t dt){
  using PF = sensor_msgs::PointField;
  switch(dt){
    case PF::INT8: return "INT8";
    case PF::UINT8: return "UINT8";
    case PF::INT16: return "INT16";
    case PF::UINT16: return "UINT16";
    case PF::INT32: return "INT32";
    case PF::UINT32: return "UINT32";
    case PF::FLOAT32: return "FLOAT32";
    case PF::FLOAT64: return "FLOAT64";
    default: return "UNKNOWN";
  }
}

static inline std::unordered_map<std::string, FieldInfo>
buildFieldMap(const sensor_msgs::PointCloud2& msg)
{
  std::unordered_map<std::string, FieldInfo> m;
  m.reserve(msg.fields.size());
  for (const auto& f : msg.fields) {
    m.emplace(f.name, FieldInfo{f.offset, f.datatype, f.count});
    // ROS_INFO("field: %-12s  offset=%u  type=%s  count=%u",
    //          f.name.c_str(), f.offset, typeStr(f.datatype), f.count);
  }
  return m;
}

class check_data
{
public:
    check_data()
    : nh_("~"),
      lidar_init_flag_(false),
      imu_init_flag_(false),
      gnss_init_flag_(false),
      lidar_frame_id_(""),
      lidar_freq_(0.0),
      lidar_count_(0),
      imu_frame_id_(""),
      imu_freq_(0.0),
      imu_count_(0),
      imu_orientation_(true),
      imu_angular_velocity_(true),
      imu_linear_acceleration_(true),
      gnss_frame_id_(""),
      gnss_freq_(0.0),
      gnss_count_(0)
    {
        //ROS_INFO("check data start!");

        //参数
        nh_.param<std::string>("lidar_topic", lidar_topic_, "/velodyne_points");
        nh_.param<std::string>("imu_topic", imu_topic_, "/imu/data");
        nh_.param<std::string>("gnss_topic", gnss_topic_, "/fix");
        nh_.param<double>("lidar_time_check", lidar_time_check_, 0.5);
        nh_.param<double>("imu_time_check", imu_time_check_, 0.05);
        nh_.param<double>("gnss_time_check", gnss_time_check_, 0.5);
        std::cout << BOLDGREEN << "[Start]: check lid  : " << BOLDYELLOW << lidar_topic_ << RESET <<std::endl;
        std::cout << BOLDGREEN << "[Start]: check imu  : " << BOLDYELLOW << imu_topic_ << RESET <<std::endl;
        std::cout << BOLDGREEN << "[Start]: check gnss : " << BOLDYELLOW << gnss_topic_ << RESET <<std::endl;

        //结果保存
        boost::filesystem::create_directories(root_dir + "/result");
        fout_imu_timerror_.open(RESULT_FILE_DIR("IMU_timerror.txt"), std::ios::out);
        fout_imu_struct_.open(RESULT_FILE_DIR("IMU_struct.txt"), std::ios::out);
        fout_lidar_timerror_.open(RESULT_FILE_DIR("Lidar_timerror.txt"), std::ios::out);
        fout_lidar_struct_.open(RESULT_FILE_DIR("Lidar_struct.txt"), std::ios::out);
        fout_lidar_outlier_.open(RESULT_FILE_DIR("Lidar_outlier.txt"), std::ios::out);
        fout_gnss_timerror_.open(RESULT_FILE_DIR("GNSS_timerror.txt"), std::ios::out);
        fout_gnss_struct_.open(RESULT_FILE_DIR("GNSS_struct.txt"), std::ios::out);
        fout_gnss_status_.open(RESULT_FILE_DIR("GNSS_status.txt"), std::ios::out);

        //订阅
        lidar_sub_ = nh_.subscribe(lidar_topic_, 1000, &check_data::lidar_callback, this);
        imu_sub_ = nh_.subscribe(imu_topic_, 1000, &check_data::imu_callback, this);
        gnss_sub_ = nh_.subscribe(gnss_topic_, 1000, &check_data::gnss_callback, this);
    }

    ~check_data(){
        std::cout << std::endl << BOLDGREEN << "[Exit]: Saving check result to : " ;
        std::cout << BOLDYELLOW << root_dir + "result" << RESET <<std::endl;

        //IMU
        fout_imu_struct_ << "IMU struct check result: " << std::endl;
        fout_imu_struct_ << "============================================" << std::endl;
        fout_imu_struct_ << "imu_topic: " << imu_topic_ << std::endl;
        fout_imu_struct_ << "frame_id: " << imu_frame_id_ << std::endl;
        fout_imu_struct_ << "freqency: " << 1 / imu_freq_ << std::endl;
        fout_imu_struct_ << "orientation: " << imu_orientation_ << std::endl;
        fout_imu_struct_ << "angular_velocity: " << imu_angular_velocity_ << std::endl;
        fout_imu_struct_ << "linear_acceleration: " << imu_linear_acceleration_ << std::endl;

        //LIDAR
        fout_lidar_struct_ << "Lidar struct check result: " << std::endl;
        fout_lidar_struct_ << "============================================" << std::endl;
        fout_lidar_struct_ << "lidar_topic: " << lidar_topic_ << std::endl;
        fout_lidar_struct_ << "frame_id: " << lidar_frame_id_ << std::endl;
        fout_lidar_struct_ << "freqency: " << 1 / lidar_freq_ << std::endl;
        fout_lidar_struct_ << "field: " << std::endl;
        for (const auto& kv : lidar_field_map_) {
            const std::string& name = kv.first;       // 字段名
            const FieldInfo& fi = kv.second;          // 该字段的偏移/类型/个数
            fout_lidar_struct_ << "  " << std::left << std::setw(14) << name << std::left << std::setw(10) << typeStr(fi.datatype) << "  offset=" << std::left << std::setw(4) << fi.offset << "  count=" << fi.count << std::endl;
        }

        //GNSS
        fout_gnss_struct_ << "GNSS struct check result: " << std::endl;
        fout_gnss_struct_ << "============================================" << std::endl;
        fout_gnss_struct_ << "gnss_topic: " << gnss_topic_ << std::endl;
        fout_gnss_struct_ << "frame_id: " << gnss_frame_id_ << std::endl;
        fout_gnss_struct_ << "freqency: " << 1 / gnss_freq_ << std::endl;
    }

private:
    void lidar_callback(const sensor_msgs::PointCloud2ConstPtr &msg)
    {
        sensor_msgs::PointCloud2 current_lidar = *msg;
        //ROS_INFO("lidar callback");
        if(!lidar_init_flag_)
        {
            lidar_init_time_ = current_lidar.header.stamp.toSec();
            lidar_frame_id_ = current_lidar.header.frame_id;
            last_lidar_time_ = current_lidar.header.stamp.toSec();
            lidar_init_flag_ = true;
            lidar_count_ = 1;
        }
        else
        {
            lidar_count_++;
            current_lidar_time_ = current_lidar.header.stamp.toSec() - lidar_init_time_;
            //时间check
            if((current_lidar.header.stamp.toSec() - last_lidar_time_ > lidar_time_check_)||(current_lidar.header.stamp.toSec() - last_lidar_time_ < 0.0) ){
                //错误记录
                ROS_ERROR("lidar time check error: lidar time diff %f",current_lidar.header.stamp.toSec() - last_lidar_time_);
                fout_lidar_timerror_ << "[timestamp]: " << std::left << std::setw(16) << current_lidar_time_ << "[timerror diff]: " << current_lidar.header.stamp.toSec() - last_lidar_time_ << std::endl;
            }else{
                lidar_freq_ = freq_update(lidar_freq_, current_lidar.header.stamp.toSec(), last_lidar_time_, lidar_count_ - 1 );
            }
            last_lidar_time_ = current_lidar.header.stamp.toSec();

            //lidar异常值
            if(!current_lidar.is_dense)
            {
                ROS_ERROR("lidar is_dense error");
                 "[outlier is_dense]: ";
                pcl::PointCloud<pcl::PointXYZI>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZI>());
                pcl::fromROSMsg(*msg, *cloud);
                // 统计 NaN/Inf、范围
                size_t total = cloud->size(), bad = 0;
                double rmin = std::numeric_limits<double>::infinity();
                double rmax = 0.0;
                for(const auto& p : cloud->points){
                    if(!pcl::isFinite(p)){ bad++; continue; }
                    double r = std::sqrt(double(p.x)*p.x + double(p.y)*p.y + double(p.z)*p.z);
                    if(std::isfinite(r)){
                    if(r < rmin) rmin = r;
                    if(r > rmax) rmax = r;
                    }
                }
                double bad_ratio = total ? (double)bad / (double)total : 0.0;
                fout_lidar_outlier_ << "[timestamp]: " << std::left << std::setw(10) << current_lidar_time_ << "[total]: " << std::left << std::setw(10) << total << "[bad]: " << std::left << std::setw(10) << bad << "[bad_ratio/%]: " << std::left << std::setw(10) << bad_ratio * 100 << "[rmin]: " << std::left << std::setw(10) << rmin << "[rmax]: " << std::left << std::setw(10) << rmax << std::endl;
            }

            //检查字段
            lidar_field_map_ = buildFieldMap(*msg);
        }
    }
    void imu_callback(const sensor_msgs::ImuConstPtr &msg)
    {
        sensor_msgs::Imu current_imu = *msg;
        //ROS_INFO("imu callback");
        if(!imu_init_flag_)
        {
            imu_init_time_ = current_imu.header.stamp.toSec();
            imu_frame_id_ = current_imu.header.frame_id;
            last_imu_time_ = current_imu.header.stamp.toSec();
            imu_init_flag_ = true;
            imu_count_ = 1;
        }
        else
        {
            imu_count_++;
            current_imu_time_ = current_imu.header.stamp.toSec() - imu_init_time_;

            //时间check
            if((current_imu.header.stamp.toSec() - last_imu_time_ > imu_time_check_)||(current_imu.header.stamp.toSec() - last_imu_time_ < 0.0) ){
                //错误记录
                ROS_ERROR("imu time check error: imu time diff %f",current_imu.header.stamp.toSec() - last_imu_time_);
                fout_imu_timerror_ << "[timestamp]: " << std::left << std::setw(16) << current_imu_time_ << "[timerror diff]: " << current_imu.header.stamp.toSec() - last_imu_time_ << std::endl;
            }else{
                imu_freq_ = freq_update(imu_freq_, current_imu.header.stamp.toSec(), last_imu_time_, imu_count_ - 1 );
            }
            last_imu_time_ = current_imu.header.stamp.toSec();

            //内容检查
            if(current_imu.orientation.x == 0.0 && current_imu.orientation.y == 0.0 && current_imu.orientation.z == 0.0 && current_imu.orientation.w == 0.0){imu_orientation_ = false;}
            if(current_imu.angular_velocity.x == 0.0 && current_imu.angular_velocity.y == 0.0 && current_imu.angular_velocity.z == 0.0){imu_angular_velocity_ = false;}
            if(current_imu.linear_acceleration.x == 0.0 && current_imu.linear_acceleration.y == 0.0 && current_imu.linear_acceleration.z == 0.0){imu_linear_acceleration_ = false;}
        }
        //ROS_INFO("currenty imu time: %f",current_imu.header.stamp.toSec());
        //ROS_INFO("currenty imu freq: %f",1 / imu_freq_);
    }
    void gnss_callback(const sensor_msgs::NavSatFixConstPtr &msg)
    {
        sensor_msgs::NavSatFix current_gnss = *msg;
        if(!gnss_init_flag_)
        {
            gnss_init_time_ = current_gnss.header.stamp.toSec();
            gnss_frame_id_ = current_gnss.header.frame_id;
            last_gnss_time_ = current_gnss.header.stamp.toSec();
            gnss_init_flag_ = true;
            gnss_count_ = 1;
            current_gnss_time_ = current_gnss.header.stamp.toSec() - gnss_init_time_;
        }
        else
        {
            gnss_count_++;
            current_gnss_time_ = current_gnss.header.stamp.toSec() - gnss_init_time_;

            //时间check
            if((current_gnss.header.stamp.toSec() - last_gnss_time_ > gnss_time_check_)||(current_gnss.header.stamp.toSec() - last_gnss_time_ < 0.0) ){
                //错误记录
                ROS_ERROR("gnss time check error: gnss time diff %f",current_gnss.header.stamp.toSec() - last_gnss_time_);
            }else{
                gnss_freq_ = freq_update(gnss_freq_, current_gnss.header.stamp.toSec(), last_gnss_time_, gnss_count_ - 1 );
            }
            last_gnss_time_ = current_gnss.header.stamp.toSec();
        }
        //status记录
        fout_gnss_status_ << "[timestamp]: " << std::left << std::setw(16) << current_gnss_time_ << "[status]: " << std::left << std::setw(10) << int(current_gnss.status.status) << "[service]: " << std::left << std::setw(10) << current_gnss.status.service << std::endl;
        //ROS_INFO("gnss callback"); 
    }

    double freq_update(double f, double cur_t_, double last_t_, int c)
    {
        if(f == 0.0){
            return (cur_t_ - last_t_);
        }else{
            return (f + ((cur_t_ - last_t_ - f) / (c)));
        }
    }

private:
    //订阅发布相关
    ros::NodeHandle nh_;
    std::string lidar_topic_, imu_topic_, gnss_topic_;
    ros::Subscriber imu_sub_, lidar_sub_, gnss_sub_;

    //数据
    double current_lidar_time_, current_imu_time_, current_gnss_time_;
    double last_lidar_time_, last_imu_time_, last_gnss_time_;
    double lidar_init_time_, imu_init_time_, gnss_init_time_;
    bool lidar_init_flag_, imu_init_flag_, gnss_init_flag_;
    double lidar_time_check_, imu_time_check_, gnss_time_check_;

    //检查内容
    //lidar
    std::string lidar_frame_id_;
    double lidar_freq_;
    int lidar_count_;
    int lidar_height_, lidar_width_, lidar_point_bytes_;
    bool lidar_bigendian_, lidar_is_dense_;
    std::unordered_map<std::string, FieldInfo> lidar_field_map_;

    //imu
    std::string imu_frame_id_;
    double imu_freq_;
    int imu_count_;
    bool imu_orientation_,imu_angular_velocity_,imu_linear_acceleration_;

    //gnss
    std::string gnss_frame_id_;
    double gnss_freq_;
    int gnss_count_;


    //结果保存
    std::string root_dir = ROOT_DIR;
    std::ofstream fout_imu_timerror_;
    std::ofstream fout_imu_struct_;
    std::ofstream fout_lidar_timerror_;
    std::ofstream fout_lidar_struct_;
    std::ofstream fout_lidar_outlier_;
    std::ofstream fout_gnss_timerror_;
    std::ofstream fout_gnss_struct_;
    std::ofstream fout_gnss_status_;
};

int main(int argc, char** argv)
{
    //ROS节点初始化
    ros::init(argc, argv, "check_data");
    std::signal(SIGINT, SigHandle);  
    
    check_data node;
    
    ros::Rate rate(5000);
    bool status = ros::ok();
    int num = 0;
    while(status)
    {
        if (flg_exit) break;
        ros::spinOnce();
        status = ros::ok();
        rate.sleep();
    }
    
    return 0;
}