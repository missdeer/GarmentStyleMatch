cmake_minimum_required(VERSION 3.21)

project(GarmentStyleMatch
    VERSION 0.1.0
    DESCRIPTION "Garment model image and hand-drawn sketch matching tool"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Widgets
)

qt_standard_project_setup(REQUIRES 6.5)

set(GSM_3RDPARTY_DIR "${CMAKE_SOURCE_DIR}/3rdparty" CACHE PATH "Third-party libraries directory")
if(EXISTS "${GSM_3RDPARTY_DIR}/CMakeLists.txt")
    add_subdirectory(3rdparty)
endif()

add_subdirectory(src)

install(TARGETS GarmentStyleMatch
    RUNTIME DESTINATION bin
    BUNDLE  DESTINATION .
)
