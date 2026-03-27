## 1. zed-open-capture install

## Description

The ZED Open Capture is a multi-platform, open-source C++ library for low-level camera and sensor capture for the ZED stereo camera family. It doesn't require CUDA and therefore can be used on many desktop and embedded platforms.

The open-source library provides methods to access raw video frames, calibration data, camera controls and raw data from the camera sensors (on ZED 2 and ZED Mini). A synchronization mechanism is provided to get the correct sensor data associated to a video frame.

**Note:** While in the ZED SDK all output data is calibrated and compensated, here the extracted raw data is not corrected by the camera and sensor calibration parameters. You can retrieve camera and sensor calibration data using the [ZED SDK](https://www.stereolabs.com/docs/video/camera-calibration/) to correct your camera data.


### Prerequisites

 * Stereo camera: [ZED 2](https://www.stereolabs.com/zed-2/), [ZED](https://www.stereolabs.com/zed/), [ZED Mini](https://www.stereolabs.com/zed-mini/)
 * Linux OS
 * GCC (v7.5+)
 * CMake (v3.1+)

### Install prerequisites

The code is implemented majorly based on [ROS melodic](https://www.ros.org/)
* Install ROS Melodic according to [ROS official instructions](http://wiki.ros.org/melodic/Installation/Ubuntu). Additionally, install `apriltag-ros` dependency with following step.

    `$ sudo apt-get install ros-melodic-apriltag`

* Install GCC compiler and build tools

    `$ sudo apt install build-essential`

* Install CMake build system

    `$ sudo apt install cmake`

* Install HIDAPI and LIBUSB libraries:

    `$ sudo apt install libusb-1.0-0-dev libhidapi-libusb0 libhidapi-dev`

* Install OpenCV to build the examples (optional)

    `$ sudo apt install opencv-dev`

###  Launch

	$ catkin build
	$ source devel/setup.bash
	$ roslaunch zed-open-capture zed2_capture.launch

# 2. AprilSLAM install - Mapping and localization from AprilTags

## 1. Introduction

Some basic code in this repository is forked from https://github.com/ProjectArtemis/aprilslam . Thanks [*@mhkabir*](https://github.com/mhkabir) for opening the awesome code.

**Your can open this [doxygen documentation](./docs/index.html#http://) created by [*@ShaotengWu*](https://github.com/ShaotengWu) in your browser for detailed documentation.**

AprilSLAM is a package designed for fast camera pose estimation from a single or multiple AprilTags in an unstructured environment. AprilSLAM needs prior information of Apriltags for better localization performance. The system can map multiple tags in the camera's view as long as there is atleast another tag in view to estimate relative tag pose the first time. The system has been run with a forward looking ZED2 stereo camera on an AGV with a X86-based computing solutions for precise estimation of the vehicle pose. The localization FPS is nearly 30Hz. The system is implemented under ROS (Robot Operating System) for ease of integration, but should be easy to run without it as well.


![aprilslam](./aprilslam/pics/aprilslam.jpg)
We use the awesome [apriltag_ros repository](https://github.com/AprilRobotics/apriltag_ros)[1-3] to extract Apriltags and modify some interfaces. The mapping system is implemented based on GTSAM [4].

The default AprilTag family used is 36h11 with a black border of 1. A PDF of the tag family is available here : http://www.dotproduct3d.com/assets/pdf/apriltags.pdf

The package is originally developed by Chao Qu and Gareth Cross from Kumar Robotics (www.kumarrobotics.org) and M.H.Kabirm. The repository is forked from the original Apriltag SLAM and is developed and maintained by Shaoteng Wu from SJTU. (Contact me: wushaoteng@sjtu.edu.cn)

![ex1](./aprilslam/pics/aprilslam.gif)

## 2. Dependencies

The code is implemented majorly based on [ROS melodic](https://www.ros.org/), [GTSAM 4.0.2](https://github.com/borglab/gtsam) and [OpenCV 4.4.0](https://opencv.org/opencv-4-0/).

### 2.1 ROS Melodic
Install ROS Melodic according to [ROS official instructions](http://wiki.ros.org/melodic/Installation/Ubuntu). Additionally, install `apriltag-ros` dependency with following step.
```bash
#!bash
$ sudo apt-get install ros-melodic-apriltag
```


### 2.2 GTSAM 4.0.2

GTSAM [installation](https://github.com/borglab/gtsam)
```bash
#!bash
$ git clone https://github.com/borglab/gtsam.git
$ cd gtsam
$ mkdir build
$ cd build
$ cmake ..
$ make check (optional, runs unit tests)
$ sudo make install
```

### 2.3 OpenCV 4.4.0

You can install OpenCV 4.4.0 referring to this [link](https://gist.github.com/raulqf/f42c718a658cddc16f9df07ecc627be7). CUDA is not necessarily needed and you can customize your own compilation settings. Aprilslam only need some basic data structures and algorithms.

$ sudo apt-get install cmake

$ sudo apt-get install build-essential pkg-config libgtk2.0-dev libavcodec-dev libavformat-dev libjpeg-dev libswscale-dev libtiff5-dev

$ unzip opencv-4.4.0
$ cd opencv-4.4.0/
$ mkdir build
$ cd build

$ cmake -D CMAKE_BUILD_TYPE=RELEASE -D CMAKE_INSTALL_PREFIX=/usr/local -D WITH_GTK=ON -D OPENCV_GENERATE_PKGCONFIG=YES ..

$ make -j8

$ sudo make install

$ sudo vim /etc/ld.so.conf
在文件中加上一行 include /usr/local/lib
$ sudo ldconfig


## 3. Get Started


### 3.1 Modify Parameters

In `aprilslam/aprilslam/launch/slam.launch`, set proper value of following parameters according to your settings.
**Attention:** The prior information yaml should be consistent with bag data for better localization performance.

...
<node pkg="rosbag" type="play" name="bag_data" args="--clock  PATH_TO_YOUR_BAG_DATA.bag" />
pcb_rviz_viewer.cpp  pcl::io::loadPCDFile<pcl::PointXYZ>("PATH_IN_YOUR_DIR/aprilslam/map/obs_points.pcd", *cloud);
```


### 3.2 Launch
```bash
#!bash
$ catkin build
$ source devel/setup.bash
# if you use zsh:
# $ source devel/setup.zsh
$ roslaunch aprilslam slam.launch
```

### 如果找不到libmetis.so，调整一下库位置。
sudo cp /usr/local/lib/libmetis.so /usr/lib


## 4.References

Please cite the appropriate papers when using this package or parts of it in an academic publication.

1. D. Malyuta, C. Brommer, D. Hentzen, T. Stastny, R. Siegwart, and R. Brockers, “Long-duration fully autonomous operation of rotorcraft unmanned aerial systems for remote-sensing data acquisition,” Journal of Field Robotics, p. arXiv:1908.06381, Aug. 2019.
2. C. Brommer, D. Malyuta, D. Hentzen, and R. Brockers, “Long-duration autonomy for small rotorcraft UAS including recharging,” in IEEE/RSJ International Conference on Intelligent Robots and Systems, IEEE, p. arXiv:1810.05683, oct 2018.
3. J. Wang and E. Olson, "AprilTag 2: Efficient and robust fiducial detection," in ''Proceedings of the IEEE/RSJ International Conference on Intelligent Robots and Systems (IROS)'', October 2016.
4. GTSAM. https://collab.cc.gatech.edu/borg/gtsam/
