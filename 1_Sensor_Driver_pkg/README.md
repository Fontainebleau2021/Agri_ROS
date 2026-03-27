# 1_Sensor_Driver_pkg
> Latest update  --  2025.12.30
> 
> 系统：Ubuntu 20.04 , ROS Noetic
>

## 串口权限相关设置
建议永久，详见前一级的基本配置
```
ls -s /dev/ttyUSB
sudo chmod 777 /dev/ttyUSB0
sudo chmod 777 /dev/ttyUSB1

#永久权限设置
sudo gpasswd -a agv dialout #其中agv是用户名
```

## > USB摄像头
>查看USB摄像头设备
```
ls /dev/video*
```
>查看图像

（1）使用应用程序camorama
```
#Install
sudo apt-get install camorama

#Run
camorama 
camorama -d /dev/video1 #指定打开设备video1
```
（2）使用应用程序茄子（cheese）
```
#Install
sudo apt-get install cheese

#Run
cheese
cheese -d /dev/video1 #指定打开设备video1
```
>查看USB摄像头参数
```
#Install
sudo apt-get install v4l-utils

#Run
v4l2-ctl -d  /dev/video0 --all #查看设备video0的参数
```
> ROS下连接USB摄像头
```
cd ~/catkin_ws/src
git clone https://github.com/bosch-ros-pkg/usb_cam.git usb_cam
cd ..
catkin_make -DCATKIN_WHITELIST_PACKAGES=usb_cam
```
打开launch文件，设置USB摄像头的编号：
```
 <param name="video_device" value="/dev/video0" />
```
启动：
```
roslaunch usb_cam usb_cam-test.launch
```

## > GNSS_pkg
### > nmea_navsat_driver
ros自带的nmea_navsat_driver驱动，可以将串口信息转换为NMEA标准的GNSS信息。详见[nmea_navsat_driver](https://wiki.ros.org/nmea_navsat_driver)
### > 联适R60-GPS相关
>硬件相关
- 安装要求：蘑菇头上放不要有遮挡，安装位置的距离在1.2m效果最好（小车前进方向左右对称）
- 数据传输：通过VGA口进行数据传输（如果工控机没有VGA口需要准备VGA转USB连接线）
>软件相关
- GPS_pkg中：
  - gps_viewer——显示GPS轨迹:
    - 订阅话题：/fix （/NavSatFix格式）
    - 输出话题：/gps_path（/Path格式）
    - 没有rviz自动显示
  - imu_gps_localization——基于卡尔曼滤波的IMU和GPS联合定位:
  - nmea_navsat_driver——GPS相关驱动
  - 注意：如果后续运行过程中发现缺少nmea_navsat_driver驱动，可以使用下面的命令手动安装：
```
sudo apt-get install ros-****-nmea-navsat-driver libgps-dev
#其中****为ubuntu版本，例如melodic版本的安装命令为：
sudo apt-get install ros-melodic-nmea-navsat-driver libgps-dev
```
- 需要测试的相关工具
  - cutecom
```
#安装
sudo apt-get install cutecom
#界面使用
cutecom
```
>GPS实机相关操作

（1）连接GPS实机使用

1、使用cutecom来确定GPS通信的串口号，需要在setting中将波特率设置为115200，输入数据的格式需要改为CR/LF（例如在下述例子中，得到的GPS通信的串口号为/dev/ttyUSB0）

2、可以通过使用cutecom发送命令改变GPS的信号传输频率（联适GPS默认频率为1Hz），例如将GPS的信号传输频率改为10Hz的命令为：LOG GPGGA ON TIME 0.1

3、使用命令从串口读取nmea格式的GPS字符段并发布话题/nmea_sentence（/nmea_msgs/Sentence格式）：
```
rosrun nmea_navsat_driver nmea_topic_serial_reader _port:=/dev/ttyUSB0 _baud:=115200
```

4、使用nmea_navsat_driver驱动中的节点接收话题/nmea_sentence（/nmea_msgs/Sentence格式）并发布/fix话题（/NavSatFix格式）：
```
rosrun nmea_navsat_driver nmea_topic_driver
```

5、打开gps_viewer和Rviz节点，并在Rviz界面中添加/gps_path数据（可视化）：
```
rosrun gps_viewer gps_viewer
rosrun rviz rviz
```
- 其中注意Rviz中的frame，需要和gps_viewer发布的话题frame一致

（2）离线bag操作

1、检查bag中GPS信号的话题格式

2、参照（1）中的4、5进行离线测试

## > IMU_pkg
### > lmps
阿鲁比系列的IMU驱动
> 相关配置
```
#ros
sudo apt install ros-melodic-openzen-sensor

#or install by package(Not recommended)
cd ~/catskin_ws/src
git clone --recurse-submodules https://bitbucket.org/lpresearch/openzenros.git
cd ..
catkin_make
```
> 运行
```
sudo chmod 777 /dev/ttyUSB0

#节点启动
rosrun openzen_sensor openzen_sensor_node

#launch文件
roslaunch lmps run.launch
```

## > marvelmind_pkg 1.0.11
Marvelmind 超声信标的驱动。详见[marvelmind_ros](https://marvelmind.com/pics/marvelmind_ROS.pdf)
最新的marvelmind驱动见[mavelmind_nav](https://github.com/MarvelmindRobotics/marvelmind_nav-release)
> 运行
```
#数据串口/dev/ttyACM0，波特率115200
rosrun marvelmind_nav hedge_rcv_bin /dev/ttyACM0 115200
```

## > Livox_pkg
### 硬件使用：

连接网线后设置静态IP：

更改ipv4为：192.168.1.50，子网掩码设置为：255.255.255.0

### 软件相关：

- [Livox SDK2](https://github.com/Livox-SDK/Livox-SDK2)

- [Livox ROS Driver 2](https://github.com/Livox-SDK/livox_ros_driver2)-Livox device driver under Ros(Compatible with ros and ros2), support Lidar HAP and Mid-360.

>Livox SDK Install
1. Dependencies:

* [CMake 3.0.0+](https://cmake.org/)
* gcc 4.8.1+

2. Install the **CMake** using apt:

```shell
$ sudo apt install cmake
```

3. Compile and install the Livox-SDK2:

```shell
$ git clone https://github.com/Livox-SDK/Livox-SDK2.git
$ cd ./Livox-SDK2/
$ mkdir build
$ cd build
$ cmake .. && make -j
$ sudo make install
```

**Note :**  
The generated shared library and static library are installed to the directory of "/usr/local/lib". The header files are installed to the directory of "/usr/local/include".

Tips: Remove Livox SDK2:

```shell
$ sudo rm -rf /usr/local/lib/liblivox_lidar_sdk_*
$ sudo rm -rf /usr/local/include/livox_lidar_*
```
>Livox ROS Driver 2
#### 安装
只能使用sh安装，且驱动文件夹必须在ws的src目录下
```
cd ~/catkin_ws/src
git clone https://github.com/Livox-SDK/livox_ros_driver2.git 
source /opt/ros/noetic/setup.sh
./build.sh ROS1
```

#### Run
详见pkg中的README

## > Ouster_Lidar_pkg

### 硬件使用：

连接网线后设置静态IP：

更改ipv4为：192.0.2.1，子网掩码设置为：255.255.255.0

### 软件使用

-[Ouster Studio 下载](https://ouster.com/zh-cn/downloads/)

Ouster Studio 启动可能需要的依赖：
```
sudo apt-get install libpython3.7
```
-[Ouster 驱动(github)](https://github.com/ouster-lidar/ouster_example)
>Dependent
```
sudo apt install build-essential cmake libeigen3-dev libjsoncpp-dev
sudo apt install ros-melodic-ros-core ros-melodic-pcl-ros ros-melodic-tf2-geometry-msgs ros-melodic-rviz
```
>Install
```
git clone https://github.com/ouster-lidar/ouster_example.git
mkdir catkin_ws
cd catkin_ws
mkdir src
ln -s <path to ouster_example> ./src/
catkin_make -DCMAKE_BUILD_TYPE=Release
```
其中，最新版本的ouster驱动可能没有ouster_ros,需要去下载旧版本

-[ouster_lidar 旧版本驱动](https://github.com/Fontainebleau2021/ouster_lidar.git)
```
git clone https://github.com/Fontainebleau2021/ouster_lidar.git
mkdir catkin_ws
cd catkin_ws
mkdir src
ln -s <path to ouster_example> ./src/
catkin_make -DCMAKE_BUILD_TYPE=Release
```
>Run
```
source ./devel/setup.bash
roslaunch ouster_ros sensor.launch sensor_hostname:=os-122149001448.local lidar_mode:=1024x10 viz:=true metadata:=$PWD/metadata.json
```
>ROS下的设置

需要将ouster_ros中的sensor.launch文件中的timestamp_mode设置为TIME_FROM_ROS_TIME的格式
```
<arg name="timestamp_mode" default="" doc="method used to timestamp measurements; possible values: {
    TIME_FROM_INTERNAL_OSC,
    TIME_FROM_SYNC_PULSE_IN,
    TIME_FROM_PTP_1588,
    TIME_FROM_ROS_TIME
    }"/>
```

