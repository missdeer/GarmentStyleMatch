cmake_minimum_required(VERSION 3.21)

project(GarmentStyleMatch
    VERSION 0.1.0
    DESCRIPTION "Garment model image and hand-drawn sketch matching tool"
    LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
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

find_package(Qt6 6.10 REQUIRED COMPONENTS
    Concurrent
    Core
    Gui
    Qml
    Quick
    QuickControls2
    Widgets
)

if(WIN32)
    find_package(Qt6 6.10 REQUIRED COMPONENTS AxContainer)
endif()

qt_standard_project_setup(REQUIRES 6.10)

set(GSM_3RDPARTY_DIR "${CMAKE_SOURCE_DIR}/3rdparty" CACHE PATH "Third-party libraries directory")
if(EXISTS "${GSM_3RDPARTY_DIR}/CMakeLists.txt")
    add_subdirectory(3rdparty)
endif()

add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

install(TARGETS GarmentStyleMatch
    RUNTIME DESTINATION bin
    BUNDLE  DESTINATION .
)
