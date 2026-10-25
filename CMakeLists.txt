cmake_minimum_required(VERSION 3.15)
project(VideoFlowSmooth CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path OpenCV prebuilt (ubah sesuai lokasi Anda)
if(WIN32 AND NOT DEFINED OpenCV_DIR)
    set(OpenCV_DIR "C:/opencv/opencv/build" CACHE PATH "OpenCV build dir")
endif()

find_package(OpenCV REQUIRED)

add_executable(video_flow_smooth main.cpp)
target_link_libraries(video_flow_smooth PRIVATE ${OpenCV_LIBS})

if(WIN32)
    # Copy DLL OpenCV + ffmpeg (dari build\bin) ke folder exe
    file(GLOB OPENCV_DLLS "${OpenCV_DIR}/x64/vc16/bin/*.dll")
    add_custom_command(TARGET video_flow_smooth POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${OPENCV_DLLS}
            $<TARGET_FILE_DIR:video_flow_smooth>
    )
endif()



