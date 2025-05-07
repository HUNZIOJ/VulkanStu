cmake_minimum_required(VERSION 3.28)

project(LearnVulkan VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

file(GLOB SRC_LIST "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cc")

find_program(GLSLC_PROGRAM glslc REQUIRED) #加载这个程序,如果没找到,那就是你没有配置环境变量
execute_process(COMMAND ${GLSLC_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/assets/shader/vertex.vert -o ${CMAKE_CURRENT_SOURCE_DIR}/assets/shader/vert.spv)
execute_process(COMMAND ${GLSLC_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/assets/shader/fragment.frag -o ${CMAKE_CURRENT_SOURCE_DIR}/assets/shader/frag.spv)

file(GLOB ASSETS ${CMAKE_CURRENT_SOURCE_DIR}/assets)
file(COPY ${ASSETS} DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/build)

add_definitions(-DDEBUG)

link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib "D:\\VulkanSDK\\1.4.309.0\\Lib") # 测试环境直接copy到全局的目录中去，每次创建新项目就直接copy这个CMakeLists.txt，也不需要改什么东西

link_libraries(glfw3)

# 链接vulkan的库
link_libraries(vulkan-1)


set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/build/bin)

add_executable(LearnVulkan ${SRC_LIST})