# 多传感器自动标定工具（课程设计）

本目录实现任务书要求：**基于 Qt 的 Python 图形界面**，封装 **ROS 相机内参标定**、**多激光雷达外参/拼接**（统一坐标系下的拼接点云）以及 **相机–激光雷达联合流程**（推荐对接拼接点云），并支持 **实时数据** 与 **ROS Bag** 思路。

---

## 推荐方案（课程未指定时，按此装机）

假定：**Ubuntu 22.04 + ROS2 Humble**。其它发行版请将命令中的 `humble` 换成你的 distro。

### 1）系统依赖（apt）

```bash
sudo apt update
sudo apt install -y \
  python3-pip \
  ros-humble-camera-calibration \
  ros-humble-image-transport \
  ros-humble-pcl-ros \
  ros-humble-pcl-conversions \
  ros-humble-tf2-tools \
  ros-humble-rviz2
```

### 2）创建工作空间并拉取第三方包（与课程推荐的默认 YAML 一致）

```bash
mkdir -p ~/calib_ws/src
cd ~/calib_ws/src

# 多雷达外参（NDT/ICP，两路点云）
git clone https://github.com/pixmoving-moveit/multi_lidar_calibration_ros2.git

# 相机–雷达融合演示（话题在包内 config/config.yaml 里改）
git clone https://github.com/LCAS/camera_lidar_humble.git

# 本仓库中的「拼接」功能包：可复制或软链到 src（把路径换成你的 project 位置）
cp -r "/path/to/课程作业/project/ros2_ws/src/multi_lidar_merge" ./

# LCAS 融合包依赖 Python 库（在其 README 中有说明，可按需安装）
pip install -r camera_lidar_humble/requirements.txt

cd ~/calib_ws
source /opt/ros/humble/setup.bash
rosdep install --from-paths src -y --ignore-src   # 若缺依赖再按需安装
colcon build --symlink-install
source install/setup.bash
```

编译成功后，在 GUI 的 **Source 列表** 里第二行填写：

`~/calib_ws/install/setup.bash`

（请写成绝对路径。）

### 3）各模块对应关系

| 功能 | 推荐实现 | 说明 |
|------|-----------|------|
| 相机内参 | `sudo apt install ros-humble-camera-calibration` | GUI 调 `cameracalibrator`，无需额外克隆 |
| 多雷达拼接 | 本仓库 `multi_lidar_merge` | TF 将各雷达变到同一 `target_frame` 后合并为 `/merged_points` |
| 双雷达外参 | [multi_lidar_calibration_ros2](https://github.com/pixmoving-moveit/multi_lidar_calibration_ros2) | 默认 launch：`multi_lidar_calibration_ndt.launch.xml`，通过 `input/source_pointcloud`、`input/target_pointcloud` 指定两路话题 |
| 相机 + 点云 | [camera_lidar_humble](https://github.com/LCAS/camera_lidar_humble) | 包名 `camera_lidar_fusion`，launch：`launch.py`。**请在源码中编辑** `config/config.yaml`，把 `lidar_sub_topic` 设为 `/merged_points`（或你的拼接话题），`image_sub_topic` 与相机一致 |

### 4）更严格的相机–雷达外参标定（可选）

若作业要求「独立外参标定工具」而非简单融合演示，建议在 Linux 中按官方文档安装 **[direct_visual_lidar_calibration](https://github.com/koide3/direct_visual_lidar_calibration)**（依赖较多，支持 Docker）。该工具以命令行/流程为主，与本 GUI 的 `ros2 launch` 封装可并行使用：GUI 负责统一环境与拼接点云，标定在独立终端按文档执行。

---

## `data/标定数据`（振华 IGV 示例）

目录中含 **`metadata.yaml`**（ROS2 录包索引）、内外参 Python/YAML、以及 **可与 metadata 同放的 `*.db3`**。录包话题特点：

- 图像为 **`sensor_msgs/msg/CompressedImage`**（如 `/s_gmsl_5/image_raw/compressed`），**无**原始 `Image`；GUI 内参页需勾选 **「CompressedImage：先 republish」**。
- 多路 **`PointCloud2`**：`/rslidar_points_*`、`/iv_points_*`、`/livox/...`。

**一键对齐话题**：工具栏 **「载入数据集预设…」** → 选 `config/presets/igv_sensor_record.yaml`。再将 **Bag 路径** 设为含 `metadata.yaml` 的文件夹（或 `.db3` 文件）。

**离线校验（无需 ROS）**：

```bash
cd project
python scripts/validate_dataset.py data/标定数据
python -m unittest tests.test_dataset_metadata -v
```

Ubuntu 上回放前建议安装：`sudo apt install ros-humble-image-transport`（提供 `image_transport republish`）。

## 目录结构

| 路径 | 说明 |
|------|------|
| `main.py` | GUI 入口：`python main.py` |
| `calibration_tool/` | PyQt6 界面、进程封装、各标定页签 |
| `config/default.yaml` | 默认参数（已填推荐第三方包名） |
| `config/presets/igv_sensor_record.yaml` | 与 `data/标定数据/metadata.yaml` 对齐的预设 |
| `config/user.yaml` | 保存配置后覆盖加载（已预填与推荐一致，可改） |
| `scripts/validate_dataset.py` | 校验 metadata 与预设话题是否一致 |
| `tests/test_dataset_metadata.py` | 单元测试（解析 metadata） |
| `ros2_ws/src/multi_lidar_merge/` | ROS2 包：多雷达点云拼接 |

## Python 界面（任意系统）

```bash
cd project
pip install -r requirements.txt
python main.py
```

在 **Windows** 上可只跑界面；真正执行 `ros2` 命令建议在 **WSL2/Ubuntu** 中启动 GUI，或在「Shell 前缀」中配置调用 WSL。

如果你在 Windows 上运行 GUI，请在 `config/user.yaml` / 界面中设置：

```yaml
ros:
  setup_scripts:
    - /opt/ros/humble/setup.bash
  shell_prefix: wsl -d Ubuntu -- bash -lc
```

## 编译仅拼接包（最小验证）

```bash
cd project/ros2_ws
source /opt/ros/humble/setup.bash
colcon build --packages-select multi_lidar_merge
source install/setup.bash
```

## 一键构建完整 ROS2 workspace

如果你还没有安装 `multi_lidar_calibration_ros2` 和 `camera_lidar_humble`，可以在 WSL 中运行：

```bash
cd project/ros2_ws
bash build_workspace.sh
source install/setup.bash
```

该脚本会自动拉取：
- `https://github.com/pixmoving-moveit/multi_lidar_calibration_ros2.git`
- `https://github.com/LCAS/camera_lidar_humble.git`

并构建整个 workspace，使下面的三个功能都可用。

## 一键启动项目功能

构建完成后，如果你想一次性启动 bag 回放、图像桥接、点云拼接、多雷达标定和相机–雷达联合，可运行：

```bash
cd project
bash start_all.sh
```

该脚本会自动：
- `source /opt/ros/humble/setup.bash`
- `source ros2_ws/install/setup.bash`
- `ros2 bag play data/标定数据 --clock`
- `ros2 run image_transport republish compressed raw ...`
- `ros2 run multi_lidar_merge merge_clouds ...`
- `ros2 launch multi_lidar_calibration multi_lidar_calibration_ndt.launch.xml ...`
- `ros2 launch camera_lidar_fusion launch.py -r image:=/s_gmsl_5/image_raw -r points:=/merged_points`

日志保存在 `project/logs/` 中。

按 Ctrl+C 会停止所有已启动的进程。

## 建议操作流程

1. **相机内参** → GUI「相机」页启动标定器，保存 yaml。  
2. **（可选）双雷达外参** → 用 NDT launch 估计两雷达相对位姿，更新 TF/URDF。  
3. **拼接** → `multi_lidar_merge` 发布 `/merged_points`。  
4. **相机 + 拼接点云** → 修改 `camera_lidar_humble` 的 `config/config.yaml`，再 GUI「联合」页启动 `launch.py`。

## 与任务书的对应关系

- **环境搭建**：YAML + GUI 配置 `setup.bash` 链。
- **算法复现**：相机用官方包；雷达与联合侧用上述推荐开源包（可替换）。
- **系统集成**：拼接输出统一话题供下游使用。
- **Bag / 实时**：相机与联合页签支持 Bag 路径（按界面选项）。
