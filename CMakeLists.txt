project(moos-hello)

cmake_minimum_required(VERSION 2.8)

aux_source_directory(. SRC_LIST)
add_executable(${PROJECT_NAME} ${SRC_LIST})

# Need C++11 b/c of thread and chrono libraries
set(CMAKE_CXX_FLAGS "-std=c++11")

find_package(MOOS 10)
include(FindProtobuf)
find_package(Protobuf REQUIRED)

include_directories(${MOOS_INCLUDE_DIRS} ${MOOS_DEPEND_INCLUDE_DIRS} ${PROTOBUF_INCLUDE_DIR})
target_link_libraries(${PROJECT_NAME} ${MOOS_LIBRARIES} ${MOOS_DEPEND_LIBRARIES} ${PROTOBUF_LIBRARY})
