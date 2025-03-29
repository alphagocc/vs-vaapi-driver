cmake_minimum_required(VERSION 3.10.0)
project(vs-vaapi VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)


add_definitions(-DFIFO_DATATYPE=void*)

aux_source_directory(src SRC)
aux_source_directory(src/base SRC_BASE)

add_library(vs-vaapi SHARED ${SRC} ${SRC_BASE})

target_link_options(vs-vaapi PRIVATE "-Wl,-Bsymbolic")
#add_executable(vdec_demo vdec-demo.cpp)
set_target_properties(vs-vaapi PROPERTIES PREFIX "")
set_target_properties(vs-vaapi PROPERTIES OUTPUT_NAME "vs_drv_video")

include_directories(libcsi_hal/include/csi_hal_vcodec)
include_directories(3rdparty/verisilicon/include)
#target_link_directories(vdec_demo PRIVATE libcsi_hal/lib)
target_link_directories(vs-vaapi PRIVATE 3rdparty/verisilicon/lib)

target_link_libraries(vs-vaapi PRIVATE OMX.hantro.VC8000D.video.decoder)
#target_link_libraries(vdec_demo PRIVATE hal_vdec)