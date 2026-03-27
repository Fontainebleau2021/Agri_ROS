# 2_Tools_pkg
> Latest update  --  2025.12.30
> 
> 系统：Ubuntu 20.04 , ROS Noetic
>

## > convert_to_velodyne
A ros tool for converting Robosense pointcloud to Velodyne pointcloud format, which can be directly used for downstream algorithm, such as LOAM, LEGO-LOAM, LIO-SAM, etc.

### Currently support:

#### 1. [robosense XYZIRT] to [velodyne XYZIRT / XYZIR / XYZI]:
RS-16, RS-32, RS-Ruby, RS-BP and RS-Helios LiDAR point cloud.

#### 2. [robosense XYZI] to [velodyne XYZIR]:
RS-16 and RS-Ruby LiDAR point cloud, More LiDAR model support is coming soon. 

#### 3. 国产镭神C16激光雷达

## Useage

### Related configuration
```
sudo apt-get install ros-noetic-velodyne-pcl
sudo apt-get install ros-noetic-velodyne-pointcloud
```

### Data input

#### 1. XYZIRT input
For **XYZIRT** format point clouds from `/rslidar_points` (Notice that, you need the latest 
[rslidar_sdk](https://github.com/RoboSense-LiDAR/rslidar_sdk) driver to get this type of point cloud):
```
rosrun rs_to_velodyne rs_to_velodyne XYZIRT XYZIRT
# or
rosrun rs_to_velodyne rs_to_velodyne XYZIRT XYZIR
# or
rosrun rs_to_velodyne rs_to_velodyne XYZIRT XYZI
``` 
The output point clouds are **XYZIRT** / **XYZIR** / **XYZI** point cloud `/velodyne_points` in Velodyne's format.

#### 2. XYZI input
For **XYZI** format point clouds from `/rslidar_points`:
```
rosrun rs_to_velodyne rs_to_velodyne XYZI XYZIR
``` 
The output point clouds are **XYZIR** point cloud `/velodyne_points` in Velodyne's format.


### Subscribes
`/rslidar_points`: sensor_msgs.PointCloud2, from Robosense LiDAR.

### Publishes
`/velodyne_points`: sensor_msgs.PointCloud2, the frame_id is `velodyne`.

## > gps_viewer
接受话题为/fix(sensor_msgs/NavSatFix)的数据，并以起始点为原点，以东北地坐标规则，转换为局部坐标
> 相关配置
```
sudo apt-get install ros-noetic-nmea-msgs
```
> 运行
```
roslaunch gps_viewer gps_viz.launch
```
## > map_related
### > map_load功能包
地图加载
- 参数修改：
    - map文件夹为二维栅格地图加载文件夹，其中png/pgm文件为地图文件，yaml文件为配置文件，其中：
        - image: map.png　　#文件名
        - resolution: 0.050000　　#地图分辨率 单位：米/像素
        - origin: [-49.0286, -107.401, 0.0]   #图像左下角在地图坐标下的坐标
        - negate: 0    #是否应该颠倒 白：自由/黑：的语义(阈值的解释不受影响)
        - occupied_thresh: 0.65   #占用概率大于此阈值的像素被认为已完全占用
        - free_thresh: 0.196   #用率小于此阈值的像素被认为是完全空闲的
    - launch文件中的tf转换参数为不同frame的静态转换
- 运行：
```
roslaunch map_load map_load.launch 
#or 在rviz中显示
roslaunch map_load map_load_rviz.launch 
```
### > pcd_to rviz
将本地PCD的地图文件显示在Rivz中

### > publish_pointcloud ————将三维pcd地图压缩为二维栅格地图并输出
- 参数修改：
    - pcd文件目录————demo.launch中的path
    - 压缩的相关参数在octomaptransform.launch中：
        - resolution 地图分辨率
        - sensor_model/max_range 压缩的范围
        - pointcloud_max_z和pointcloud_min_z 压缩的高度限制
- 运行

```
roslaunch publish_pointcloud demo.launch 
```

## > agr_service
通过服务进行节点启动和结束
- 运行
```
#客户端测试
rosrun agr_service agr_service
#服务端测试
rosrun agr_service server.py
```