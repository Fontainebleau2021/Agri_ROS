# 农业机器人 Agri_ROS 代码仓库

本仓库面向农业机器人场景，构建一个基于 **Ubuntu 20.04 + ROS Noetic** 的模块化算法框架，用于统一管理传感器驱动、数据处理、定位与 SLAM、感知、规划与控制等核心功能模块，并扩展支持基于 AI 模型的智能感知能力。

该框架继承农业机器人系统常见的分层组织方式，并在此基础上引入面向未来的智能感知与多传感器融合扩展能力。

---

## 1. 项目概述

本仓库旨在构建一个**结构清晰、可扩展、工程与科研兼顾**的农业机器人算法集成平台，主要目标包括：

* 提供统一的 ROS 工作空间组织规范
* 模块化管理农业机器人核心算法组件
* 支持完整算法链路：

  * 传感器驱动
  * 数据预处理与质量检测
  * 标定
  * 定位与 SLAM
  * 感知（含 AI 方法）
  * 规划与控制
* 为后续多传感器融合与智能化方法提供扩展基础

---

## 2. 系统环境

* 操作系统：Ubuntu 20.04
* ROS 版本：ROS Noetic

---

## 3. 仓库结构

```bash
.
├── 1_Sensor_Driver_pkg/             # 传感器驱动模块（LiDAR / IMU / GNSS / Camera）
├── 2_Tools_pkg/                    # 工具模块（数据处理、调试、预处理）
├── 3_Calibration_pkg/              # 标定模块（外参/时间同步等）
├── 4_Positioning_SLAM_pkg/         # 定位与 SLAM 模块（LiDAR / LIO / 多传感器）
├── 5_Perception_pkg/               # 传统感知模块
├── 6_Planning_pkg/                 # 规划模块
├── 7_Control_pkg/                  # 控制模块
├── 8_Networksocket_pkg/            # 通信模块
├── 9_Intelligent_Perception_pkg/   # 智能感知与 AI 模型模块
```

---

## 4. 模块说明

### 4.1 传感器驱动模块（1_Sensor_Driver_pkg）

提供各类传感器接口与数据发布节点，包括：

* 激光雷达（LiDAR）
* 惯性测量单元（IMU）
* GNSS
* 相机

---

### 4.2 工具模块（2_Tools_pkg）

用于支撑数据处理、调试与系统验证的基础工具集合，包括：

* 数据完整性检测
* 点云预处理
* 调试与可视化工具

#### ✔ check_data 功能包

该工具包用于多传感器数据质量检查与点云处理，包含以下核心节点：

* `check_data_node`
  用于检测 LiDAR / IMU / GNSS 数据的时间连续性与数据结构合法性

* `lidar_clean_node`
  去除点云中的 NaN 与 Inf 点

* `lidar_convert_xyzi_node`
  将点云统一转换为标准 XYZI / XYZ 格式，并输出字段信息

示例：

```bash
roslaunch check_data check_data.launch
roslaunch check_data lidar_clean.launch
roslaunch check_data lidar_convert_xyzi.launch
```

---

### 4.3 标定模块（3_Calibration_pkg）

用于多传感器系统的标定与对齐，包括：

* 外参标定（LiDAR-IMU / LiDAR-Camera 等）
* 时间同步与校正

---

### 4.4 定位与 SLAM 模块（4_Positioning_SLAM_pkg）

本模块为系统核心，负责机器人定位与环境建图，支持：

* LiDAR 里程计（LiDAR Odometry）
* 激光-惯性融合（LIO）
* SLAM 系统

设计上支持：

* 退化场景下的鲁棒定位
* 基于可观测性的估计稳定性分析
* 面向多传感器融合的自适应扩展

---

### 4.5 感知模块（5_Perception_pkg）

传统感知方法，包括：

* 目标检测
* 特征提取
* 基础语义理解

---

### 4.6 规划模块（6_Planning_pkg）

实现路径规划与轨迹生成：

* 全局路径规划
* 局部轨迹优化

---

### 4.7 控制模块（7_Control_pkg）

实现机器人运动控制：

* 底盘控制
* 控制接口封装

---

### 4.8 通信模块（8_Networksocket_pkg）

用于系统间通信与数据交互：

* Socket 通信
* 多设备协同

---

### 4.9 智能感知模块（9_Intelligent_Perception_pkg）

该模块用于引入 AI 模型与智能感知能力，作为传统感知模块的重要补充，主要包括：

* 基础视觉模型（检测 / 分割 / 表征学习）
* 视觉-语言模型（Vision-Language Models）
* 大模型工具接口（LLM-based tools）

建议结构：

```bash
9_Intelligent_Perception_pkg/
├── model_zoo/        # 模型配置与索引
├── wrappers/         # ROS接口封装
├── pipelines/        # 感知处理流程
└── docs/             # 模型说明文档
```

---

## 5. 编译方式

```bash
cd ~/your_workspace
catkin_make
source devel/setup.bash
```

---

## 6. 快速使用示例

运行数据检查工具：

```bash
roslaunch check_data check_data.launch
```

---

## 7. 设计原则

本仓库遵循以下设计原则：

* **模块化设计**：各功能模块解耦，支持独立开发与扩展
* **ROS 兼容性**：统一 ROS 接口规范
* **算法与系统解耦**：核心算法逻辑与 ROS 封装分离
* **可扩展性**：支持多传感器融合与智能感知扩展
* **工程与科研统一**：兼顾系统实现与算法研究

---

## 8. 后续发展方向

* 基于可观测性的自适应定位与 SLAM
* LIO 框架增强（结合 CT-ICP / 因子图方法）
* 稳定结构地图（Stable Map）作为先验约束
* AI 感知与定位系统深度融合

---

## 9. 说明

* 各模块可独立扩展，新增功能需遵循目录结构规范
* 外部依赖建议通过脚本或文档统一管理
* 推荐将详细说明文档放置于 `docs/` 或各子模块目录中
