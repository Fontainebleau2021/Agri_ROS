# 7_Control_pkg
> Latest update  --  2025.12.30
> 
> 系统：Ubuntu 20.04 , ROS Noetic
>

## > bunker_description
导航地图规划中松灵bunker模型加载

## > navigation（ A* + TEB ）
导航功能包，功能包内容详见功能包中的readme.
- 相关配置：ceres-solver , g2o(上一级目录有详细配置教程)
```
sudo apt-get install ros-noetic-costmap-converter
sudo apt-get install ros-noetic-mbf-msgs

```
- 参数修改：
    - 导航相关修改详见navigation/config/navigation/navigation_params.yaml文件
    - teb规划下速度限制参数详见navigation/config/teb/teb_local_planner_params.yaml文件
- 运行

```
roslaunch navigation navigation_node.launch 
```
运行后使用goal指针工具可以指定小车的终点。
