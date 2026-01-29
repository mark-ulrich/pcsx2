# CMakeLists for resync-ng

cmake_minimum_required(VERSION 3.10)
project(resync-ng VERSION 1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(SOURCES
        Resync.cpp
        Resync.h
        ResyncConfigDialog.cpp
        ResyncConfigDialog.h
        ResyncConfig.ui
        ResyncAboutDialog.cpp
        ResyncAboutDialog.h
        ResyncAboutDialog.ui
)

add_library(resync-ng STATIC ${SOURCES})
find_package(Qt6 REQUIRED COMPONENTS Network Widgets)

target_link_libraries(resync-ng PRIVATE Qt6::Network Qt6::Widgets KDAB::kddockwidgets)

target_compile_definitions(resync-ng PRIVATE ${PCSX2_DEFS})

target_include_directories(resync-ng PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(resync-ng PRIVATE thirdparty)
target_include_directories(resync-ng PRIVATE ../3rdparty/ccc/src .. ../pcsx2 ../pcsx2-qt)