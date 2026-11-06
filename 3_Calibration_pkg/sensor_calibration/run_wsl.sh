#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"

# Install Python dependencies
python3 -m pip install --upgrade pip
python3 -m pip install -r requirements.txt

# Build ROS2 package
cd ros2_ws
source /opt/ros/humble/setup.bash
colcon build --packages-select multi_lidar_merge
source install/setup.bash

# Start the GUI
cd ..
python3 main.py
