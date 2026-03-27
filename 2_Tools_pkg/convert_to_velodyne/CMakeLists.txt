cmake_minimum_required(VERSION 3.0.2)
project(convert_to_velodyne)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O3")
set(CMAKE_CXX_VERSION 14)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  rospy
  pcl_ros
  pcl_conversions
  velodyne_pointcloud
  std_msgs
  sensor_msgs
)

catkin_package(
  INCLUDE_DIRS include
  DEPENDS PCL
)

include_directories(
	include
	${catkin_INCLUDE_DIRS}
	${PCL_INCLUDE_DIRS}
)

link_directories(
	include
	${PCL_LIBRARY_DIRS}
)


add_executable(rs_to_velodyne src/rs_to_velodyne.cpp)
add_dependencies(rs_to_velodyne ${catkin_EXPORTED_TARGETS})
target_link_libraries(rs_to_velodyne ${catkin_LIBRARIES} ${PCL_LIBRARIES} ${OpenCV_LIBRARIES})

add_executable(ls_to_velodyne src/ls_to_velodyne.cpp)
add_dependencies(ls_to_velodyne ${catkin_EXPORTED_TARGETS})
target_link_libraries(ls_to_velodyne ${catkin_LIBRARIES} ${PCL_LIBRARIES} ${OpenCV_LIBRARIES})