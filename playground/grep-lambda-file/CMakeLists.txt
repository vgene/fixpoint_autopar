cmake_minimum_required(VERSION 3.5)
#set(CMAKE_CXX_STANDARD 11)
project(hello LANGUAGES CXX)


list(APPEND CMAKE_PREFIX_PATH "/u/ziyangx/.local/aws-lambda/lib/aws-lambda-runtime/cmake")
list(APPEND CMAKE_PREFIX_PATH "/u/ziyangx/.local/aws-lambda/lib")
list(APPEND CMAKE_PREFIX_PATH "/u/ziyangx/local_install/aws-sdk-cpp/install/lib/")
list(APPEND CMAKE_PREFIX_PATH "/u/ziyangx/local_install/aws-sdk-cpp/install/lib/cmake")

# include_directories("/u/ziyangx/.local/aws-lambda/include")

find_package(aws-lambda-runtime REQUIRED)
find_package(AWSSDK REQUIRED COMPONENTS s3)
add_executable(${PROJECT_NAME} "main.cpp")
target_link_libraries(${PROJECT_NAME} PUBLIC 
    AWS::aws-lambda-runtime
    ${AWSSDK_LINK_LIBRARIES})
aws_lambda_package_target(${PROJECT_NAME})
