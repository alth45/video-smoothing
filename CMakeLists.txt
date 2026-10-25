cmake_minimum_required(VERSION 3.15)
project(VideoFlowSmooth CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === Windows: fallback OpenCV prebuilt path ===
if(WIN32 AND NOT DEFINED OpenCV_DIR AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(OpenCV_DIR "C:/opencv/opencv/build" CACHE PATH "OpenCV build dir")
endif()

# === Termux detection ===
set(IS_TERMUX FALSE)
if(DEFINED ENV{TERMUX_VERSION} OR DEFINED ENV{PREFIX})
    if(DEFINED ENV{PREFIX} AND "$ENV{PREFIX}" MATCHES "com.termux")
        set(IS_TERMUX TRUE)
        message(STATUS "Detected Termux (Android) — build mode: mobile")
    endif()
endif()

find_package(OpenCV REQUIRED)
find_package(Threads REQUIRED)

add_executable(video_flow_smooth
    src/main.cpp
    src/pipeline.cpp
    src/flow_interpolator.cpp
    src/scene_detector.cpp
    src/ffmpeg_helper.cpp
    src/util.cpp
    src/banner.cpp
)
target_include_directories(video_flow_smooth PRIVATE src)
target_link_libraries(video_flow_smooth PRIVATE ${OpenCV_LIBS} Threads::Threads)

# === Windows: copy DLLs di samping .exe ===
if(WIN32)
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.21" AND DEFINED CMAKE_TOOLCHAIN_FILE)
        add_custom_command(TARGET video_flow_smooth POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:video_flow_smooth>
                $<TARGET_FILE_DIR:video_flow_smooth>
            COMMAND_EXPAND_LISTS
        )
    endif()
    if(EXISTS "${OpenCV_DIR}/x64/vc16/bin")
        file(GLOB OPENCV_DLLS "${OpenCV_DIR}/x64/vc16/bin/*.dll")
        add_custom_command(TARGET video_flow_smooth POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${OPENCV_DLLS}
                $<TARGET_FILE_DIR:video_flow_smooth>
        )
    endif()
endif()

# === Termux: link c++_shared agar ABI cocok dengan pkg opencv ===
if(IS_TERMUX)
    find_library(CXX_SHARED_LIB c++_shared)
    if(CXX_SHARED_LIB)
        message(STATUS "Termux: linking ${CXX_SHARED_LIB}")
        target_link_libraries(video_flow_smooth PRIVATE ${CXX_SHARED_LIB})
    else()
        message(WARNING "Termux: libc++_shared.so tidak ditemukan, "
                        "build mungkin crash saat runtime")
    endif()
endif()

# === Linux/Termux: install target (opsional) ===
if(UNIX AND NOT APPLE)
    install(TARGETS video_flow_smooth RUNTIME DESTINATION bin)
endif()