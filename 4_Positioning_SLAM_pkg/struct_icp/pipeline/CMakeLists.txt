# MIT License
#
# Copyright (c) 2022 Ignacio Vizzo, Tiziano Guadagnino, Benedikt Mersch, Cyrill
# Stachniss.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

add_library(struct_icp_pipeline STATIC)
add_library(struct_icp::pipeline ALIAS struct_icp_pipeline)

# Threads (for async rebuild)
find_package(Threads REQUIRED)

# --- app: run_sequence ---
add_executable(run_sequence apps/run_sequence.cpp)
target_link_libraries(run_sequence PRIVATE struct_icp::pipeline Threads::Threads)

target_sources(struct_icp_pipeline PRIVATE
  StructICP.cpp
)

# 依赖 core
target_link_libraries(struct_icp_pipeline PUBLIC struct_icp::core Threads::Threads)

# PCL for PCD I/O
find_package(PCL REQUIRED COMPONENTS io)
target_include_directories(struct_icp_pipeline PUBLIC ${PCL_INCLUDE_DIRS})
target_link_libraries(struct_icp_pipeline PUBLIC ${PCL_LIBRARIES})

set_global_target_properties(struct_icp_pipeline)
