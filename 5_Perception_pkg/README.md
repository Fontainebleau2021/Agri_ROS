# 7_Perception_pkg
> Latest update  --  2025.12.30
> 
> 系统：Ubuntu 20.04 , ROS Noetic
>
## > depth_clustering

-[depth_clustering](https://github.com/PRBonn/depth_clustering)

-[激光聚类算法-depth_clustering安装运行](https://blog.csdn.net/qq_40216084/article/details/108703496)

>Depentent
```
sudo apt install libopencv-dev libqglviewer-dev-qt5 freeglut3-dev qtbase5-dev
```
>Install
```
cd ~/catkin_ws/src
git clone https://github.com/PRBonn/depth_clustering src/depth_clustering
cd depth_clustering
mkdir build
cd build
cmake ..
make -j4
ctest -VV  # run unit tests, optional
```
>Testing dataset
```
cd ~/catkin_ws/src/depth_clustering
mkdir data/; wget http://www.mrt.kit.edu/z/publ/download/velodyneslam/data/scenario1.zip -O data/moosmann.zip; unzip data/moosmann.zip -d data/; rm data/moosmann.zip
```
下载下来的数据是.png格式的，都是深度图，总共110M左右。运行测试：
```
cd depth_clustering/build/devel/lib/depth_clustering/
./qt_gui_app #运行可视化界面
```
点击open_folder，选择刚才下载的数据目录，然后点击play，运行。

>ros环境

使用catkin_tools进行编译
```
sudo apt install python3-pip
sudo python3 -m pip install --upgrade --force-reinstall pip
sudo pip install catkin_tools
cd ~/catkin_ws
catkin build
```
注意对已编译的功能包要使用catkin build命令。

show_objects_node订阅了/velodyne_points的点云话题，运行：
```
cd catkin_depth
source devel/setup.bash
rosrun depth_clustering show_objects_node --num_beams 16 --angle 10
```

## > lidar_obstacle_detector
欧式聚类的激光点云聚类，详见pkg内的README。
- 安装
```bash
# clone the repo
cd catkin_ws/src
git clone https://github.com/SS47816/lidar_obstacle_detector.git

# install dependencies & build
cd ..
rosdep install --from-paths src --ignore-src -r -y
catkin_make # or catkin_make -DPYTHON_EXECUTABLE=/usr/bin/python3
source devel/setup.bash
```