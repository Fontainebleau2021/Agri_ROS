# 4_SLAM_pkg
> Latest update  --  2025.12.30
> 
> 系统：Ubuntu 20.04 , ROS Noetic
>
> 最新版更新了车辆模型的载入

## > PreciseSLAM
以liosam为模板改动的SLAM定位建图算法，详见功能包中的README
- 参数配置详见SLAM_pkg/lio_sam_devel/config/params.yaml文件(不同传感器的liosam算法需要修改对应后缀的yaml文件)
- 运行
```
roslaunch lio_sam run.launch
#启动相应传感器的liosam算法，例如启动livox的liosam算法
roslaunch lio_sam run_livox.launch
```
- 车辆载入,更新车辆模型可用这个网址-[Free 3D car Models](https://free3d.com/3d-models/collada-car) 
```
rosrun lio_sam car_pub.py
```

## > Tag_SLAM
Apriltag为视觉基准的SLAM算法，详见功能包中的README

## > LIO-SAM
- Dependent
```
#ros
sudo apt-get install -y ros-melodic-navigation
sudo apt-get install -y ros-melodic-robot-localization
sudo apt-get install -y ros-melodic-robot-state-publisher
#gtsam
sudo add-apt-repository ppa:borglab/gtsam-release-4.0
sudo apt install libgtsam-dev libgtsam-unstable-dev
```
- Package

- [LIO-SAM](https://github.com/TixiaoShan/LIO-SAM.git)

- [LIO-SAM with relocal](https://github.com/Fontainebleau2021/lio_sam_devel.git)
```
cd ~/catkin_ws/src
git clone https://github.com/TixiaoShan/LIO-SAM.git
#or git clone https://github.com/Fontainebleau2021/lio_sam_devel.git
cd ..
catkin_make
```

## > EVO-SLAM轨迹对比工具
>安装evo

因为ros依赖的python版本是2.7，目前最新版本的evo是支持python3.7+，在ubuntu18.04中建议通过源码安装evo。
```
python --version  //确认当前python版本是否为2，若为3建议手动切换到2在进行安装
```
（注意python和pip下载的对应关系，python2.7就用pip，python3用pip3，evo1.1.2用pip，evo高版本用pip3 ；注意ros只支持python2）
```
git clone https://github.com/MichaelGrupp/evo.git
#下载git包
cd evo
git checkout v1.12.0
#检查
sudo pip install  -i https://pypi.tuna.tsinghua.edu.cn/simple --editable . --upgrade --no-binary evo 
#解决下载慢的问题（镜像）（我这里找不到命令，所以前面加了sudo）
```

>测试evo

（1）命令行输入evo，现实如下：

<p align='center'>
    <img src="./image/evo.png" alt="drawing" width="800"/>
</p>

（2）通过源码包下自带的.txt测试文件

```
cd test/data
evo_traj kitti KITTI_00_ORB.txt KITTI_00_SPTAM.txt --ref=KITTI_00_gt.txt -p --plot_mode=xyz
```

安装成功页面：
<p align='center'>
    <img src="./image/evo_traj.png" alt="drawing" width="800"/>
</p>

>卸载evo

```
pip list              #列出pip安装的软件包
pip uninstall evo     #卸载
```

>使用指令

读取bag画出轨迹

```
evo_traj bag ROS_example.bag --all_topics -p --plot_mode=xy
```

>问题解决

 - 在mac上，plot后端不匹配python tk

```
evo_config set plot_backend tkagg
```