cmake_minimum_required(VERSION 3.16)
project(uav_gcs C CXX)

set(C_STANDARD 17)
set(C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Werror -Wconversion)

add_executable(hello src/hello.c)