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

cd "$PROJECT_ROOT"

BAG_PID=$(start_background "bag_play" ros2 bag play "$BAG_DIR" --clock --read-ahead-queue-size 1000)
sleep 4

BRIDGE_PID=$(start_background "image_bridge" ros2 run image_transport republish compressed raw in:=/s_gmsl_5/image_raw/compressed out:=/s_gmsl_5/image_raw)
sleep 2

TF_PID=$(start_background "static_tf" python3 "$PROJECT_ROOT/scripts/publish_static_tf.py" "$BAG_DIR/8-igv_sensor_parameters-环视相机激光雷达内外参.py")
sleep 2

MERGE_PID=$(start_background "merge_clouds" ros2 run multi_lidar_merge merge_clouds --ros-args -p input_topics_csv:="/rslidar_points_1,/rslidar_points_2,/rslidar_points_3,/rslidar_points_4,/iv_points_10,/iv_points_11,/livox/lidar_172_168_1_200,/livox/lidar_172_168_1_201" -p output_topic:="/merged_points" -p target_frame:="base_link" -p publish_rate:=10.0)
sleep 2

CALIB_PID=$(start_background "lidar_calib" ros2 launch multi_lidar_calibration multi_lidar_calibration_ndt.launch.xml input/source_pointcloud:=/rslidar_points_1 input/target_pointcloud:=/rslidar_points_2)
sleep 2

FUSION_PID=$(start_background "fusion" ros2 run camera_lidar_fusion lidar_fusion --ros-args -p image_sub_topic:=/s_gmsl_5/image_raw -p lidar_sub_topic:=/merged_points)

echo
cat <<'EOF'
Started all services.
Logs:
  bag_play -> logs/bag_play.log
  image_bridge -> logs/image_bridge.log
  static_tf -> logs/static_tf.log
  merge_clouds -> logs/merge_clouds.log
  lidar_calib -> logs/lidar_calib.log
  fusion -> logs/fusion.log

Press Ctrl+C to stop all processes.
EOF

wait
