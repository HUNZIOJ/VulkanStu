cmake_minimum_required(VERSION 3.28)

project(LearnVulkan VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB SRC_LIST "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")

add_definitions(-DDEBUG)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib "D:\\VulkanSDK\\1.4.309.0\\Lib") # 测试环境直接copy到全局的目录中去，每次创建新项目就直接copy这个CMakeLists.txt，也不需要改什么东西

link_libraries(glfw3)

# 链接vulkan的库
link_libraries(vulkan-1)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/build/bin)

add_executable(LearnVulkan ${SRC_LIST})