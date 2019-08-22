

cmake_minimum_required(VERSION 3.4.1)

#静态库使用没有问题，动态库使用有版本兼容问题
#add_library(
#        avcodec
#        STATIC
#        IMPORTED)
#
#set_target_properties(
#        avcodec
#        PROPERTIES
#        IMPORTED_LOCATION
#        ${CMAKE_SOURCE_DIR}/libs/${CMAKE_ANDROID_ARCH_ABI}/libavcodec.a
#)


#导入头文件
include_directories(include/)

#设置导入的需要链接库目录
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L${CMAKE_SOURCE_DIR}/libs/${CMAKE_ANDROID_ARCH_ABI}")

file(GLOB src_files *.cpp)

#aux_source_directory()

#添加待编译的库
add_library(
        native-lib
        SHARED
        ${src_files})


#链接库到目标库
target_link_libraries(
        native-lib
        #链接顺序问题
        #                avformat avcodec avfilter avutil swresample swscale

        -Wl,--start-group
        swscale avformat avcodec avfilter avutil swresample
        -Wl,--end-group
        log
        z
        android
        OpenSLES
)