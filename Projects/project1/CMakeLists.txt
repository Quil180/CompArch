cmake_minimum_required(VERSION 3.16.0)
project(SIM LANGUAGES CXX)

set(TARGET_NAME SIM)

add_executable(${TARGET_NAME}
  src/main.cpp
)

set(CMAKE_CXX_STANDARD 25)
set(CMAKE_CXX_STANDARD_REQUIRED True)
