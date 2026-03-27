#include <ros/ros.h>
#include <sensor_msgs/CameraInfo.h>
#include <sensor_msgs/Image.h>
#include <vector>

ros::Publisher camera_info_pub;
sensor_msgs::CameraInfo camera_info;

sensor_msgs::CameraInfo getCameraInfo(void)
{ // extract cameraInfo.
    sensor_msgs::CameraInfo cam;

    std::vector<double> D{-0.0394652, 0.00863897, -0.00054791, -0.00016387, -0.00461753};

    boost::array<double, 9> K = {
        530.30, 0, 658.24,
        0, 530.00, 370.78,
        0, 0, 1};

    boost::array<double, 12> P = {
        530.30, 0, 658.24, 0,
        0, 530.00, 370.78, 0,
        0, 0, 1, 0};
    boost::array<double, 9> r = {1, 0, 0, 0, 1, 0, 0, 0, 1};

////zed NO.2 param
//    std::vector<double> D{-0.001594, 0.000387, 0.000223, -0.001245, 0.000000};
//
//    boost::array<double, 9> K = {
//            532.692494, 0.000000, 629.853062,
//            0.000000, 533.190736, 363.061454,
//            0.000000, 0.000000, 1.000000};
//
//    boost::array<double, 12> P = {
//            535.990253, 0.000000, 623.426437, 0.000000,
//            0.000000, 535.990253, 366.408554, 0.000000,
//            0.000000, 0.000000, 1.000000, 0.000000};
//    boost::array<double, 9> r = {0.999999, 0.000168, 0.001032,
//    -0.000165, 0.999996, -0.002721,
//    -0.001032, 0.002721, 0.999996};

    cam.height = 720;
    cam.width = 1280;
    cam.distortion_model = "plumb_bob";
    cam.D = D;
    cam.K = K;
    cam.P = P;
    cam.R = r;
    cam.binning_x = 0;
    cam.binning_y = 0;
    return cam;
}

void imageCallback(const sensor_msgs::ImageConstPtr &img)
{
    camera_info.header.stamp = ros::Time::now();
    camera_info_pub.publish(camera_info);
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "zed_camera_info");
    ros::NodeHandle nh;
    camera_info_pub = nh.advertise<sensor_msgs::CameraInfo>("/zed2/camera_info", 1, true);
    ros::Subscriber image_sub = nh.subscribe("/zed2/image_raw", 1, imageCallback);
    camera_info = getCameraInfo();
    ros::spin();



    return 0;
}
