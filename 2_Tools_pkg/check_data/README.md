# AGR-SLAM-2025
> Latest update  --  2025.12.03
> 
> 系统：Ubuntu 20.04 , ROS Noetic
>

# check_data

数据通用检查与点云清洗工具集，包含三个 ROS 节点：

- `check_data_node`：对雷达/IMU/GNSS 数据做时间连续性与结构检查并生成报告。
- `lidar_clean_node`：清理点云中的 NaN/Inf 点，保持原有字段格式发布。
- `lidar_convert_xyzi_node`：基于 `PointCloud2` 架构打印点云 schema，并输出统一的 XYZI（或 XYZ）点云。

## 功能概览
- **时间与频率检查**：对点时间戳做前后差值校验（参数阈值可调），并估计频率。
- **结构检查**：记录雷达字段信息、IMU 关键字段是否为零、GNSS 状态值。
- **异常点统计**：当雷达消息 `is_dense` 为 false 时，统计坏点比例与半径范围。
- **点云清洗与格式化**：`lidar_clean_node` 仅剔除 NaN/Inf；`lidar_convert_xyzi_node` 会在过滤后输出 height=1 的 XYZI/XYZ 点云，并将 intensity 统一转成 float32。

## 主要节点与参数

### check_data_node（src/check_data/src/check_data.cpp）
- 订阅：`lidar_topic`（默认 `/velodyne_points`）、`imu_topic`（默认 `/imu/data`）、`gnss_topic`（默认 `/fix`）。
- 时间阈值：`lidar_time_check`、`imu_time_check`、`gnss_time_check`。
- 启动方式：`roslaunch check_data check_data.launch`（默认加载 `config/config.yaml`，覆盖上述参数）。
- 输出结果：节点退出时在 `result/` 下生成/追加  
  - `Lidar_timerror.txt`、`IMU_timerror.txt`、`GNSS_timerror.txt`（时间连续性）  
  - `Lidar_struct.txt`、`IMU_struct.txt`、`GNSS_struct.txt`（结构与频率汇总）  
  - `Lidar_outlier.txt`（`is_dense`=false 时的坏点统计）  
  - `GNSS_status.txt`（status/service 记录）

### lidar_clean_node（src/check_data/src/lidar_clean.cpp）
- 订阅：`lidar_topic`（默认 `/ouster/points`）
- 发布：`lidar_clean_topic`（默认 `/ouster/points_clean`）
- 参数：`debug_info` 控制频率/范围日志（launch 默认 false）
- 特点：使用 `PointCloud2` 迭代器按索引剔除 NaN/Inf，并保留原始所有字段，输出 `is_dense=true`。
- 启动：`roslaunch check_data lidar_clean.launch`

### lidar_convert_xyzi_node（src/check_data/src/lidar_convert_xyzi.cpp）
- 订阅：`lidar_topic`（默认 `/ouster/points`）
- 发布：`lidar_clean_topic`（默认 `/ouster/points_convert`，launch 设置为 `/ouster/points_convert_xyzi`）
- 参数：`debug_info` 控制 schema/统计日志
- 特点：
  - 打印 `PointCloud2` fields、point_step、是否含 intensity/ring/time 等信息。
  - 仅保留有限点，按原类型读取 intensity（支持 float32/uint16/int16/uint8/int8），统一写为 float32。
  - 输出非组织化点云（height=1）并保持 `header`。
- 启动：`roslaunch check_data lidar_convert_xyzi.launch`

## 文件结构
- `launch/`：三个节点的示例启动文件。
- `config/config.yaml`：`check_data_node` 默认参数。
- `src/`：节点实现。
- `include/check_data/color.h`：终端彩色输出宏。
- `result/`：运行后生成的检查结果文本。

## 构建与运行
1) 将本包放入 catkin 工作空间 `src/` 下，执行：
```bash
catkin_make
source devel/setup.bash
```
2) 数据检查：
```bash
roslaunch check_data check_data.launch
```
3) 点云清洗：
```bash
roslaunch check_data lidar_clean.launch
```
4) 点云转 XYZI/格式检查：
```bash
roslaunch check_data lidar_convert_xyzi.launch
```

## 自定义
- 如需修改订阅/发布话题或时间阈值，可在对应 launch 文件内调整 `<param>`，或直接改 `config/config.yaml`。
- 结果文件默认写入包内 `result/` 目录，必要时可在代码中调整 `ROOT_DIR` 定义或将目录做符号链接。 


