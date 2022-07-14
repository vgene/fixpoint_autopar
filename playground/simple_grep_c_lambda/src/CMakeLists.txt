cmake_minimum_required(VERSION 3.3)
set(CMAKE_CXX_STANDARD 11)
project(app LANGUAGES CXX)

# set c++ to gclang++
# set cc to gclang
set(CMAKE_CXX_COMPILER gclang++)
set(CMAKE_C_COMPILER gclang)

list(APPEND CMAKE_PREFIX_PATH "/u/ziyangx/local_install/aws-sdk-cpp/install/lib/")
list(APPEND CMAKE_PREFIX_PATH "/u/ziyangx/local_install/aws-sdk-cpp/install/lib/cmake")
list(APPEND CMAKE_PREFIX_PATH "/u/ziyangx/.local/aws-lambda/lib/aws-lambda-runtime/cmake")
# list(APPEND CMAKE_PREFIX_PATH "/u/ziyangx/local_install/aws-sdk-cpp/install/lib/aws-crt-cpp/cmake")

message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
set(BUILD_SHARED_LIBS ON CACHE STRING "Link to shared libraries by default.")

#Load required services/packages: This basic example uses S3.
# find_package(AWSSDK REQUIRED COMPONENTS )
find_package(AWSSDK REQUIRED COMPONENTS s3 lambda)

add_executable(${PROJECT_NAME} "grep.cpp") #Add app's main starting file.

# set_compiler_flags(${PROJECT_NAME})
# set_compiler_warnings(${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} ${AWSSDK_LINK_LIBRARIES})
