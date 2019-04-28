cmake_minimum_required(VERSION 3.0.2)

project("rpc-protobuf-cpp")

add_compile_options(-std=c++14)

# 添加cmake_modules
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")

# 添加库文件输出目录
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# 添加可执行文件输出目录
set(EXECUTABLE_OUTPUT_PATH  ${PROJECT_SOURCE_DIR}/bin)

################# folly #############
find_package(Folly REQUIRED)
message("find Folly include: ${FOLLY_INCLUDE_DIR}")
message("find Folly libraries: ${FOLLY_LIBRARIES}")

################# wangle #############
find_package(wangle REQUIRED)
message("find wangle include: ${WANGLE_INCLUDE_DIR}")
message("find wangle libraries: ${WANGLE_LIBRARIES}")

################# protobuf #############
find_package(Protobuf REQUIRED)
message("find Protobuf include: ${PROTOBUF_INCLUDE_DIRS}")
message("find Protobuf libraries: ${PROTOBUF_LIBRARIES}")


include_directories(
        ${PROJECT_SOURCE_DIR}
        ${PROTOBUF_INCLUDE_DIRS}
        ${WANGLE_INCLUDE_DIR}
)

add_library(pbrpc
        net/PbRpcServer/PbRpcServer.cpp
        net/PbRpcClient/PbRpcClient.cpp
)

target_link_libraries(pbrpc
        ${FOLLY_LIBRARIES}
        ${WANGLE_LIBRARIES}
        ${PROTOBUF_LIBRARIES}
)