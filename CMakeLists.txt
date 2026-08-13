cmake_minimum_required(VERSION 3.16)
project(uav_gcs C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Werror -Wconversion)
add_compile_options(-fsanitize=address,undefined)
add_link_options(-fsanitize=address,undefined)

enable_testing()

add_library(uavlink STATIC
    libuavlink/src/crc.c
)
target_include_directories(uavlink PUBLIC libuavlink/include)

add_executable(test_crc libuavlink/tests/test_crc.c)
target_link_libraries(test_crc PRIVATE uavlink)
add_test(NAME crc COMMAND test_crc)