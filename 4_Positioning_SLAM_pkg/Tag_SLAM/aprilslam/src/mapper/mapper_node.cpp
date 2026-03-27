#include "aprilslam/mapper_node.h"
#include "aprilslam/utils.h"
#include <iostream>
#include <fstream>
#include <stdlib.h>

namespace aprilslam
{
    MapperNode::MapperNode(const ros::NodeHandle &nh, const std::string &frame_id)
        : nh_(nh),
          sub_tags_(nh_.subscribe("/aprilslam_apriltags", 1, &MapperNode::TagsCb, this)),
          sub_cinfo_(nh_.subscribe("/zed2_left/camera_info", 1, &MapperNode::CinfoCb, this)),
          frame_id_(frame_id),
          mapper_(0.04, 1),
          map_("36h11", 0.45),
          tag_viz_(nh, "apriltags_map"),
          pose_cnt_(0)

    {
        nh_.getParam(nh_.getNamespace() + "/mapper/use_tag_prior_info", use_tag_prior_info_);
        nh_.getParam(nh_.getNamespace() + "/mapper/tag_prior_info_path", tag_prior_info_path_);

        // Publisher settings.
        pub_cam_trajectory_ = nh_.advertise<nav_msgs::Path>("cam_trajectory", 1);
        pub_obj_pointcloud_ = nh_.advertise<sensor_msgs::PointCloud>("object_points", 1);
        cam_trajectory_.header.frame_id = frame_id_;

        if (use_tag_prior_info_)
        {
            // Load prior information
            ROS_INFO("\033[1;32m----> Use prior information of apriltags. \033[0m");
            ROS_INFO("\033[1;32m----> The yaml file path: %s \033[0m", tag_prior_info_path_.c_str());
            tag_prior_info_node_ = YAML::LoadFile(tag_prior_info_path_);
            
            // If there is no valid information
            if (!tag_prior_info_node_["tags"].IsDefined())
            {
                ROS_ERROR("Invalid yaml file!");
                ROS_ERROR("Do not use prior information of apriltags.");
                exit(0);
            }
            else if (tag_prior_info_node_["tags"].IsNull())
            {
                ROS_ERROR("No tags information in yaml file!");
                ROS_ERROR("Do not use prior information of apriltags.");
                exit(0);
            }
            // If there is valid info.
            else
            {
                size_t tags_num = tag_prior_info_node_["tags"].size();
                ROS_INFO("\033[1;32m----> Valid yaml file loaded.\033[0m");
                ROS_INFO("\033[1;32m----> Prior apriltags number: %d \033[0m", tags_num);

                // load prior information
                auto tag_iter_begin = tag_prior_info_node_["tags"].begin();
                auto tag_iter_end = tag_prior_info_node_["tags"].end();
                for (auto tag_iter = tag_iter_begin; tag_iter != tag_iter_end; tag_iter++)
                {
                    auto tag_node = *tag_iter;
                    size_t id = tag_node["id"].as<size_t>();
                    std::vector<double> translation = tag_node["translation"].as<std::vector<double>>();
                    std::vector<double> rotation = tag_node["rotation"].as<std::vector<double>>();

                    geometry_msgs::Pose tag_prior_pose;
                    tag_prior_pose.position.x = translation[0];
                    tag_prior_pose.position.y = translation[1];
                    tag_prior_pose.position.z = translation[2];
                    tag_prior_pose.orientation.x = rotation[0];
                    tag_prior_pose.orientation.y = rotation[1];
                    tag_prior_pose.orientation.z = rotation[2];
                    tag_prior_pose.orientation.w = rotation[3];

                    tag_prior_poses_.insert(std::pair<int, geometry_msgs::Pose>(id, tag_prior_pose));
                }

                // Pass prior info to mapper(Optimizer) and tag_map(Manager)
                mapper_.UpdateTagsPriorInfo(tag_prior_poses_);
                map_.UpdateTagsPriorInfo(tag_prior_poses_);
            }
        }
        else
        {
            ROS_INFO(" Do not use prior information of apriltags. ");
        }

        // Visualization settings
        tag_viz_.SetColor(aprilslam::GREEN);
        tag_viz_.SetAlpha(0.75);
    }

    void MapperNode::TagsCb(const aprilslam::ApriltagsConstPtr &tags_c_msg)
    {
        if (tags_c_msg->apriltags.empty())
        {
            ROS_WARN_THROTTLE(1, "No tags detected.");
            return;
        }

        // Do nothing if camera info not received
        if (!model_.initialized())
        {
            ROS_WARN_THROTTLE(1, "No camera info received");
            return;
        }
        mapper_.InitCameraParams(model_.fullIntrinsicMatrix(), model_.distortionCoeffs());

        // Do nothing if there are no good tags close to the center of the image
        std::vector<Apriltag> tags_c_good;
        tags_c_good.clear();
        if (!GetGoodTags(tags_c_msg->apriltags, &tags_c_good))
        {
            ROS_WARN_THROTTLE(1, "No good tags detected.");
            return;
        }

        if (!tags_c_good.empty())
        {
            map_.UpdateTagsw(tags_c_good);
        }

        // The poses of apriltag in tags_c_good are in camera frame rather than world(tag) frame
        geometry_msgs::Pose pose;
        if (!map_.EstimatePose(tags_c_good, model_.fullIntrinsicMatrix(), model_.distortionCoeffs(), &pose))
        {
            ROS_WARN_THROTTLE(1, "No 2D-3D correspondence.");
            return;
        }

        //是否保存数据文本?
        SavePoseData(false, pose);

        // Visualization of Apriltags corner points
        sensor_msgs::PointCloud obj_pointcloud_viz = map_.obj_pointcloud_viz();
        obj_pointcloud_viz.header.frame_id = frame_id_;
        obj_pointcloud_viz.header.stamp = ros::Time::now();
        pub_obj_pointcloud_.publish(obj_pointcloud_viz);

        // Publish camera to world transform
        std_msgs::Header header;
        header.stamp =  ros::Time::now();
        header.frame_id = frame_id_;

        geometry_msgs::Vector3 translation;
        translation.x = pose.position.x;
        translation.y = pose.position.y;
        translation.z = pose.position.z;

        geometry_msgs::TransformStamped transform_stamped;
        transform_stamped.header = header;
        transform_stamped.header.stamp = ros::Time::now();
        transform_stamped.child_frame_id = "camera";
        transform_stamped.transform.translation = translation;
        transform_stamped.transform.rotation = pose.orientation;

        tf_broadcaster_.sendTransform(transform_stamped);

        // Publish visualisation markers
        tag_viz_.SetColor(aprilslam::RED);
        tag_viz_.PublishApriltagsMarker(map_.tags_w(), frame_id_, tags_c_msg->header.stamp);

        // Publish prior tag info from /.yaml, this part is good.
        std::vector<aprilslam::Apriltag> tags_w_prior = map_.tags_w_prior();
        tag_viz_.SetColor(aprilslam::YELLOW);
        tag_viz_.PublishPriorApriltagsMarker(tags_w_prior, frame_id_, tags_c_msg->header.stamp);
    }

//    void MapperNode::TagsCb(const aprilslam::ApriltagsConstPtr &tags_c_msg)
//    {
//        if (tags_c_msg->apriltags.empty())
//        {
//            ROS_WARN_THROTTLE(1, "No tags detected.");
//            return;
//        }
//
//        // Do nothing if camera info not received
//        if (!model_.initialized())
//        {
//            ROS_WARN_THROTTLE(1, "No camera info received");
//            return;
//        }
//        mapper_.InitCameraParams(model_.fullIntrinsicMatrix(), model_.distortionCoeffs());
//
//        // Do nothing if there are no good tags close to the center of the image
//        std::vector<Apriltag> tags_c_good;
//        tags_c_good.clear();
//        if (!GetGoodTags(tags_c_msg->apriltags, &tags_c_good))
//        {
//            ROS_WARN_THROTTLE(1, "No good tags detected.");
//            return;
//        }
//
//        // Initialize map by adding the first tag (the best tag_c_good transform to world frame) that is not on the edge of the image
//        // 只在初始化的时候执行一次
//        if (!map_.init())
//        {
//            map_.AddFirstTag(tags_c_good.front());
//            ROS_INFO("AprilMap initialized.");
//        }
//
//        // Do nothing if no pose can be estimated
//        // The poses of apriltag in tags_c_good are in camera frame rather than world(tag) frame
//        geometry_msgs::Pose pose;
//        if (!map_.EstimatePose(tags_c_good, model_.fullIntrinsicMatrix(), model_.distortionCoeffs(), &pose))
//        {
//            ROS_WARN_THROTTLE(1, "No 2D-3D correspondence.");
//            return;
//        }
//
//        // Visualization of Apriltags corner points
//        sensor_msgs::PointCloud obj_pointcloud_viz = map_.obj_pointcloud_viz();
//        obj_pointcloud_viz.header.frame_id = frame_id_;
//        obj_pointcloud_viz.header.stamp = ros::Time::now();
//        pub_obj_pointcloud_.publish(obj_pointcloud_viz);
//
//        // Now that with the initial pose calculated, we can do some mapping
//        /* -------------------------------------------------------------------------- */
//        /*          toggle to add/remove const velocity motion model factors          */
//        /* -------------------------------------------------------------------------- */
//        mapper_.AddPose(pose, cam_velocity_);
////        mapper_.AddPose(pose);
//
//        // Add factors. Including:
//         // BetweenFactor -- Camera and Apriltags.
//         // GenericProjectionFactor --  Visual projection of corners
//         // RangeFactor -- Apriltag size constraint between its corner and center
//        mapper_.AddFactors(tags_c_good);
//
//        // This will only add init esitimate and prior factor of new landmarks
//        mapper_.AddLandmarks(tags_c_good);
//
//        Frontend_debug(false, tags_c_good);
//
//        if (mapper_.init())
//        {
//            // Update by iSAM2
//            mapper_.Optimize(10);
//
//            // Get latest estimates from mapper and put into map
//            mapper_.Update(&map_, &pose);
//
//            // Calculate velocity
//            cam_velocity_ = map_.getVelocity();
//            map_.UpdateCurrentCamPose(pose);
//
//            // Continuous localization result. Published by pub_cam_trajectory_.
//            geometry_msgs::PoseStamped cam_pose_stamped;
//            cam_pose_stamped.header.stamp =  ros::Time::now();
//            cam_pose_stamped.header.frame_id = frame_id_;
//            cam_pose_stamped.pose = pose;
//            cam_trajectory_.poses.push_back(cam_pose_stamped);
//            pub_cam_trajectory_.publish(cam_trajectory_);
//
//            //是否保存数据文本?
//            SavePoseData(false, pose);
//        }
//        else
//        {
//            // No projectionFactor will be added before initialization. Due to no tags in tags_w_all_
//            mapper_.InitCameraParams(model_.fullIntrinsicMatrix(), model_.distortionCoeffs());
//
//            // This will add first landmark at origin and fix scaler first pose and
//            // first landmark
//            mapper_.Initialize(map_.first_tag());
//        }
//
//        // Publish camera to world transform
//        std_msgs::Header header;
//        header.stamp =  ros::Time::now();
//        header.frame_id = frame_id_;
//
//        geometry_msgs::Vector3 translation;
//        translation.x = pose.position.x;
//        translation.y = pose.position.y;
//        translation.z = pose.position.z;
//
//        geometry_msgs::TransformStamped transform_stamped;
//        transform_stamped.header = header;
//        transform_stamped.header.stamp = ros::Time::now();
//        transform_stamped.child_frame_id = "camera";
//        transform_stamped.transform.translation = translation;
//        transform_stamped.transform.rotation = pose.orientation;
//
//        tf_broadcaster_.sendTransform(transform_stamped);
//
//        // Publish visualisation markers
//        tag_viz_.SetColor(aprilslam::RED);
//        tag_viz_.PublishApriltagsMarker(map_.tags_w(), frame_id_, tags_c_msg->header.stamp);
//
//        // Publish prior tag info from /.yaml, this part is good.
//        std::vector<aprilslam::Apriltag> tags_w_prior = map_.tags_w_prior();
//        tag_viz_.SetColor(aprilslam::YELLOW);
//        tag_viz_.PublishPriorApriltagsMarker(tags_w_prior, frame_id_, tags_c_msg->header.stamp);
//    }

    void MapperNode::CinfoCb(const sensor_msgs::CameraInfoConstPtr &cinfo_msg)
    {
        if (model_.initialized())
        {
            sub_cinfo_.shutdown();
            ROS_INFO("%s: %s", nh_.getNamespace().c_str(), "Camera initialized");
            return;
        }
        model_.fromCameraInfo(cinfo_msg);
    }

    bool MapperNode::GetGoodTags(const std::vector<Apriltag> tags_c, std::vector<Apriltag> *tags_c_good)
    {
        std::vector<Apriltag> tags_c_tmp = tags_c;
        std::sort(tags_c_tmp.begin(), tags_c_tmp.end(), [](Apriltag tag1, Apriltag tag2)
        {   double tag1_dis = pow(tag1.pose.position.x, 2) + pow(tag1.pose.position.y, 2) + pow(tag1.pose.position.z, 2);
            double tag2_dis = pow(tag2.pose.position.x, 2) + pow(tag2.pose.position.y, 2) + pow(tag2.pose.position.z, 2);
            return tag1_dis < tag2_dis; });   // 排序有3种表达方式，此处用的是 lambda 表达式排序。https://blog.csdn.net/m0_37316917/article/details/86618256

        for (const Apriltag &tag_c : tags_c_tmp)
        {
            //Only use the five smallest id tags because tag with smaller id is closer
            // TODO: Custom your own filter condition here!
            if (tags_c_good->size() >= 6) {
                break;
            }
            tags_c_good->push_back(tag_c);
//            //检测tag_c是不是处于整个图像的中心位置
//            //TODO: Custom your own filter condition here!
//            if (IsInsideImageCenter(tag_c.center.x, tag_c.center.y, model_.cameraInfo().width, model_.cameraInfo().height, 5))
//            {
//            }
        }
        return !tags_c_good->empty();
    }

    void MapperNode::SavePoseData (bool SaveFile,const geometry_msgs::Pose &pose)
    {
        if(SaveFile){
            std::ofstream outfile("/home/ubuntu/catkin_ws_221028_WithoutGraph/data/data.txt", std::ios::app);   //定义输出文件流对象，打开磁盘文件"test.txt" 。如果没有该文件则创建
            if(!outfile){          //如果打开失败，outfile返回0值
                std::cerr<<"open error!"<<std::endl;
                exit(1);
            }
            outfile << "& current time: ["<< ros::Time::now() << "], [" << pose.position.x << "," <<pose.position.y << "," << pose.position.z << "]" <<std::endl;
            outfile.close();         //关闭磁盘文件"test.txt"
            std::cout << "& current time: ["<< ros::Time::now() << "], [" << pose.position.x << "," <<pose.position.y << "," << pose.position.z << "]" <<std::endl;
        }
    }

    void MapperNode::Frontend_debug(bool SaveFile, const std::vector<Apriltag> tags_c_good)
    {
        if(SaveFile){
            std::ofstream outfile("/home/zhangwei/Desktop/frontend_data.txt", std::ios::app);   //定义输出文件流对象，打开磁盘文件"test.txt" 。如果没有该文件则创建
            if(!outfile){          //如果打开失败，outfile返回0值
                std::cerr<<"open error!"<<std::endl;
                exit(1);
            }
            outfile << "**********current cont is: "<< Mapper::pose_cnt << "**********" << std::endl;
            for (const Apriltag tagc_debug : tags_c_good)
            {
                double distance = sqrt(pow(tagc_debug.pose.position.x, 2) + pow(tagc_debug.pose.position.y, 2) + pow(tagc_debug.pose.position.z, 2));
                outfile << "**tag.id = "<< tagc_debug.id << " distance is = "<< distance << std::endl;
            }
            outfile.close();         //关闭磁盘文件"test.txt"
        }
    }

} // namespace aprilslam
