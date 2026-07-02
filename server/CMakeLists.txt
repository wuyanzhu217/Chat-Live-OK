cmake_minimum_required(VERSION 3.16)
project(chatlive-server VERSION 0.1 LANGUAGES CXX)

# Placeholder root CMake — chatlive binary will live under chatlive/
# Build (when implemented):
#   cmake -S server -B build/server -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build/server

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message(STATUS "chatlive-server: scaffold only — implement chatlive/ then add_subdirectory")

# Uncomment when chatlive/ is ready:
# add_subdirectory(chatlive)
