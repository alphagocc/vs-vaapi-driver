cmake_minimum_required(VERSION 3.10.0)
project(vs-vaapi VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)

add_definitions(-DFIFO_DATATYPE=void*)

aux_source_directory(src SRC)

add_library(vs-vaapi SHARED ${SRC})

add_executable(vdec_demo vdec-demo.cpp)

include_directories(libcsi_hal/include/csi_hal_vcodec)

include_directories(3rdparty/verisilicon/include)
include_directories(3rdparty/verisilicon/include/common)
include_directories(3rdparty/verisilicon/include/config)

target_link_directories(vdec_demo PRIVATE libcsi_hal/lib)
target_link_directories(vs-vaapi PRIVATE libcsi_hal/lib)

target_link_libraries(vs-vaapi PRIVATE hal_vdec vmem)
target_link_libraries(vdec_demo PRIVATE hal_vdec)