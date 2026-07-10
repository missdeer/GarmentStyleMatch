set(GSM_SOURCES
    main.cpp
    core/MatchController.cpp
    core/CandidateListModel.cpp
    core/GalleryListModel.cpp
    core/PhotoListModel.cpp
    core/PptPageListModel.cpp
    core/PptStyleExtractor.cpp
    core/ImageMetadata.cpp
)

set(GSM_HEADERS
    core/MatchController.h
    core/CandidateListModel.h
    core/GalleryListModel.h
    core/PhotoListModel.h
    core/PptPageListModel.h
    core/PptStyleExtractor.h
    core/ImageMetadata.h
)

# Some localized MSVC installations emit /showIncludes output in a code page
# that Ninja cannot parse reliably.  Keep project-header dependencies explicit
# so a class layout change can never leave main.cpp or another source object
# built against an older layout.
list(TRANSFORM GSM_HEADERS PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/"
     OUTPUT_VARIABLE GSM_HEADER_DEPENDENCIES)
set_property(SOURCE ${GSM_SOURCES} APPEND PROPERTY
    OBJECT_DEPENDS "${GSM_HEADER_DEPENDENCIES}"
)

set(GSM_QML_FILES
    qml/Main.qml
    qml/components/HeaderBar.qml
    qml/components/CandidatePanel.qml
    qml/components/MainImageView.qml
    qml/components/GalleryPanel.qml
    qml/components/PptPreviewPanel.qml
    qml/components/StyleGalleryPanel.qml
    qml/components/ConfirmBar.qml
    qml/components/PathPickerRow.qml
    qml/components/ImagePropertiesPanel.qml
)

set(GSM_PLATFORM_SOURCES)
if(WIN32)
    list(APPEND GSM_PLATFORM_SOURCES GarmentStyleMatch.rc)
endif()

qt_add_executable(GarmentStyleMatch WIN32 MACOSX_BUNDLE
    ${GSM_SOURCES}
    ${GSM_HEADERS}
    ${GSM_PLATFORM_SOURCES}
)

qt_add_qml_module(GarmentStyleMatch
    URI GarmentStyleMatch
    VERSION 1.0
    QML_FILES ${GSM_QML_FILES}
)

target_include_directories(GarmentStyleMatch PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(LibArchive REQUIRED)

target_link_libraries(GarmentStyleMatch PRIVATE
    Qt6::Concurrent
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    LibArchive::LibArchive
    $<$<PLATFORM_ID:Windows>:Qt6::AxContainer>
)

set_target_properties(GarmentStyleMatch PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER  com.garmentstylematch.app
    MACOSX_BUNDLE_BUNDLE_VERSION  ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

# ---- 开发期:写一个 qt.conf,让运行时直接去 Qt 安装目录找 qml/plugins ----
# 这样不用每次 windeployqt 拷一大堆文件,也避开了 windeployqt 6.11 对
# qt_add_qml_module 项目漏拷 QtQuick.Controls 顶层 qmldir 的 bug。
get_target_property(_gsm_qmake_exe Qt6::qmake IMPORTED_LOCATION)
get_filename_component(_gsm_qt_bin_dir "${_gsm_qmake_exe}" DIRECTORY)
get_filename_component(_gsm_qt_prefix  "${_gsm_qt_bin_dir}" DIRECTORY)
message(STATUS "GarmentStyleMatch: Qt prefix = ${_gsm_qt_prefix}")

set(_gsm_qt_conf "${CMAKE_BINARY_DIR}/qt.conf.in")
file(WRITE "${_gsm_qt_conf}"
"[Paths]\nPrefix = ${_gsm_qt_prefix}\n")

add_custom_command(TARGET GarmentStyleMatch POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_gsm_qt_conf}"
            "$<TARGET_FILE_DIR:GarmentStyleMatch>/qt.conf"
    COMMENT "Writing qt.conf → Prefix=${_gsm_qt_prefix}"
    VERBATIM
)

# ---- 打包期:cmake --install 时用 Qt 自己的部署脚本,把 qml/plugins 拷齐 ----
qt_generate_deploy_qml_app_script(
    TARGET GarmentStyleMatch
    OUTPUT_SCRIPT GSM_DEPLOY_SCRIPT
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${GSM_DEPLOY_SCRIPT})
