#!/usr/bin/env bash
set -eo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"

cd "$PROJECT_ROOT"

# Set required environment variables before sourcing ROS
export AMENT_TRACE_SETUP_FILES=${AMENT_TRACE_SETUP_FILES:-}
export AMENT_PYTHON_EXECUTABLE=${AMENT_PYTHON_EXECUTABLE:-python3}
export COLCON_TRACE=${COLCON_TRACE:-}

# Source ROS2 environment
source /opt/ros/humble/setup.bash
source ros2_ws/install/setup.bash

# Ensure log directory exists
mkdir -p logs

echo "[INFO] Starting data sources and TF publisher..."

# Start bag playback in background
ros2 bag play data/标定数据/rosbag2_2023_12_07-01_11_42_0.db3 --clock --read-ahead-queue-size 1000 >logs/bag_play.log 2>&1 &
BAG_PID=$!
echo "[INFO] Bag play started, PID=$BAG_PID"

# Wait a bit for bag to start
sleep 4

# Start image bridge in background
ros2 run image_transport republish compressed raw in:=/s_gmsl_5/image_raw/compressed out:=/s_gmsl_5/image_raw >logs/image_bridge.log 2>&1 &
BRIDGE_PID=$!
echo "[INFO] Image bridge started, PID=$BRIDGE_PID"

# Wait a bit
sleep 2

# Start static TF publisher in background
python3 scripts/publish_static_tf.py data/标定数据/8-igv_sensor_parameters-环视相机激光雷达内外参.py >logs/static_tf.log 2>&1 &
TF_PID=$!
echo "[INFO] Static TF publisher started, PID=$TF_PID"

echo
echo "Data sources started successfully!"
echo "PIDs: bag=$BAG_PID, bridge=$BRIDGE_PID, tf=$TF_PID"
echo "Logs: logs/bag_play.log, logs/image_bridge.log, logs/static_tf.log"
echo
echo "Press Ctrl+C to stop all processes"

# Wait for processes
trap 'echo "Stopping processes..."; kill $BAG_PID $BRIDGE_PID $TF_PID 2>/dev/null || true; wait 2>/dev/null; exit 0' SIGINT SIGTERM EXIT
wait