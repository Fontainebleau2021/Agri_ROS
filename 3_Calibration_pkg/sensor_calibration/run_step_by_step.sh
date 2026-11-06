#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
WS_ROOT="$PROJECT_ROOT/ros2_ws"
BAG_DIR="$PROJECT_ROOT/data/标定数据"
LOG_DIR="$PROJECT_ROOT/logs"

if [[ ! -d "$WS_ROOT" ]]; then
  echo "[ERROR] ROS2 workspace not found: $WS_ROOT" >&2
  exit 1
fi
if [[ ! -e "$BAG_DIR" ]]; then
  echo "[ERROR] Bag path not found: $BAG_DIR" >&2
  exit 1
fi

mkdir -p "$LOG_DIR"

set +u
source /opt/ros/humble/setup.bash

if [[ ! -d "$WS_ROOT/install/multi_lidar_calibration" || ! -d "$WS_ROOT/install/camera_lidar_fusion" || ! -d "$WS_ROOT/install/multi_lidar_merge" ]]; then
  echo "[INFO] ROS2 workspace missing required packages; attempting build..."
  bash "$WS_ROOT/build_workspace.sh"
fi

source "$WS_ROOT/install/setup.bash"
set -u

export RMW_IMPLEMENTATION=rmw_fastrtps_cpp

trap 'echo "Stopping all child processes..."; kill "$BAG_PID" 2>/dev/null || true; kill "$BRIDGE_PID" 2>/dev/null || true; kill "$TF_PID" 2>/dev/null || true; kill "$MERGE_PID" 2>/dev/null || true; kill "$CALIB_PID" 2>/dev/null || true; kill "$FUSION_PID" 2>/dev/null || true; wait 2>/dev/null; exit 0' SIGINT SIGTERM EXIT

start_background() {
  local name="$1"
  shift
  echo "[INFO] Starting $name..."
  "$@" >"$LOG_DIR/${name}.log" 2>&1 &
  local pid=$!
  echo "[INFO] $name pid=$pid"
  echo "$pid"
}

# 检查是否为交互式终端，如果是则等待用户输入，否则自动继续
wait_for_user() {
  if [[ -t 0 ]]; then
    echo "按Enter继续..."
    read
  else
    echo "非交互式环境，自动继续..."
    sleep 1
  fi
}

cd "$PROJECT_ROOT"

echo "=== 步骤1: 启动 ROS2 bag 播放器 ==="
echo "这将播放标定数据包，发布传感器话题"
BAG_PID=$(start_background "bag_play" ros2 bag play "$BAG_DIR" --clock --read-ahead-queue-size 1000)
echo "等待4秒让bag播放器启动..."
sleep 4
echo "检查bag播放日志:"
tail -10 "$LOG_DIR/bag_play.log"
wait_for_user

echo "=== 步骤2: 启动图像桥接器 ==="
echo "这将将压缩图像转换为原始图像"
BRIDGE_PID=$(start_background "image_bridge" ros2 run image_transport republish compressed raw in:=/s_gmsl_5/image_raw/compressed out:=/s_gmsl_5/image_raw)
echo "等待2秒..."
sleep 2
echo "检查图像桥接日志:"
tail -5 "$LOG_DIR/image_bridge.log"
wait_for_user

echo "=== 步骤3: 启动静态TF发布器 ==="
echo "这将发布15个静态变换到base_link坐标系"
TF_PID=$(start_background "static_tf" python3 "$PROJECT_ROOT/scripts/publish_static_tf.py" "$BAG_DIR/8-igv_sensor_parameters-环视相机激光雷达内外参.py")
echo "等待2秒..."
sleep 2
echo "检查TF发布日志:"
tail -5 "$LOG_DIR/static_tf.log"
wait_for_user

echo "=== 步骤4: 启动点云合并节点 ==="
echo "这将订阅8个激光雷达话题，变换到base_link并合并"
MERGE_PID=$(start_background "merge_clouds" ros2 run multi_lidar_merge merge_clouds --ros-args -p input_topics_csv:="/rslidar_points_1,/rslidar_points_2,/rslidar_points_3,/rslidar_points_4,/iv_points_10,/iv_points_11,/livox/lidar_172_168_1_200,/livox/lidar_172_168_1_201" -p output_topic:="/merged_points" -p target_frame:="base_link" -p publish_rate:=10.0)
echo "等待2秒..."
sleep 2
echo "检查点云合并日志:"
tail -10 "$LOG_DIR/merge_clouds.log"
wait_for_user

echo "=== 步骤5: 启动多激光雷达标定节点 ==="
echo "这将使用NDT算法标定激光雷达之间的外参"
CALIB_PID=$(start_background "lidar_calib" ros2 launch multi_lidar_calibration multi_lidar_calibration_ndt.launch.xml input/source_pointcloud:=/rslidar_points_1 input/target_pointcloud:=/rslidar_points_2)
echo "等待2秒..."
sleep 2
echo "检查标定日志:"
tail -10 "$LOG_DIR/lidar_calib.log"
wait_for_user

echo "=== 步骤6: 启动相机激光雷达融合节点 ==="
echo "这将进行YOLO-based的早期融合"
FUSION_PID=$(start_background "fusion" ros2 run camera_lidar_fusion lidar_fusion --ros-args -p image_sub_topic:=/s_gmsl_5/image_raw -p lidar_sub_topic:=/merged_points)
echo "等待2秒..."
sleep 2
echo "检查融合日志:"
tail -10 "$LOG_DIR/fusion.log"
wait_for_user

echo
echo "=== 所有服务已启动 ==="
echo "现在可以观察各个日志文件或使用ROS2工具检查话题:"
echo "  - ros2 topic list"
echo "  - ros2 topic echo /merged_points"
echo "  - ros2 topic echo /camera_lidar_fusion/result"
echo
echo "日志文件位置:"
echo "  bag_play -> logs/bag_play.log"
echo "  image_bridge -> logs/image_bridge.log"
echo "  static_tf -> logs/static_tf.log"
echo "  merge_clouds -> logs/merge_clouds.log"
echo "  lidar_calib -> logs/lidar_calib.log"
echo "  fusion -> logs/fusion.log"
echo
echo "按Ctrl+C停止所有进程"

wait