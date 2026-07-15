set(GSM_SOURCES
    main.cpp
    core/MatchController.cpp
    core/HttpDownloader.cpp
    core/CandidateListModel.cpp
    core/GalleryListModel.cpp
    core/PhotoListModel.cpp
    core/PptPageListModel.cpp
    core/PptStyleExtractor.cpp
    core/ImageMetadata.cpp
    core/GarmentMatcher.cpp
    core/MatchResultStore.cpp
    core/SQLiteDB.cpp
    core/SQLiteStatement.cpp
    core/WindowsMlExecutionProvider.cpp
)

set(GSM_HEADERS
    core/MatchController.h
    core/HttpDownloader.h
    core/CandidateListModel.h
    core/GalleryListModel.h
    core/PhotoListModel.h
    core/PptPageListModel.h
    core/PptStyleExtractor.h
    core/ImageMetadata.h
    core/GarmentMatcher.h
    core/MatchResultStore.h
    core/SQLiteDB.h
    core/SQLiteStatement.h
    core/WindowsMlExecutionProvider.h
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
    qml/components/OutputImagePreview.qml
    qml/components/GalleryPanel.qml
    qml/components/PptPreviewPanel.qml
    qml/components/StyleGalleryPanel.qml
    qml/components/MatchPanel.qml
    qml/components/ClearableTextField.qml
    qml/components/IconButton.qml
    qml/components/PathPickerRow.qml
    qml/components/ImagePropertiesPanel.qml
)

set(GSM_IMAGE_FILES
    "${CMAKE_SOURCE_DIR}/GarmentStyleMatch.png"
)
set_source_files_properties(${GSM_IMAGE_FILES} PROPERTIES
    QT_RESOURCE_ALIAS "images/GarmentStyleMatch.png"
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
    RESOURCES ${GSM_IMAGE_FILES}
)

target_include_directories(GarmentStyleMatch PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(GarmentStyleMatch PRIVATE
    Qt6::Concurrent
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    LibArchive::LibArchive
    gsm_onnxruntime_headers
    ${OpenCV_LIBS}
    gsm_sqlite
    gsm_windowsml_api
    $<$<PLATFORM_ID:Windows>:Qt6::AxContainer>
)

if(WIN32)
    add_custom_command(TARGET GarmentStyleMatch POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${GSM_ONNXRUNTIME_RUNTIME_ROOT}/cuda"
                "$<TARGET_FILE_DIR:GarmentStyleMatch>/onnxruntime/cuda"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${GSM_ONNXRUNTIME_RUNTIME_ROOT}/directml"
                "$<TARGET_FILE_DIR:GarmentStyleMatch>/onnxruntime/directml"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${GSM_WINDOWSML_RUNTIME_ROOT}"
                "$<TARGET_FILE_DIR:GarmentStyleMatch>/onnxruntime/windowsml"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${GSM_WINDOWSML_RUNTIME_ROOT}/Microsoft.Windows.AI.MachineLearning.dll"
                "$<TARGET_FILE_DIR:GarmentStyleMatch>"
        COMMENT "Copying ONNX Runtime binaries"
        VERBATIM
    )
else()
    add_custom_command(TARGET GarmentStyleMatch POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${GSM_ONNXRUNTIME_RUNTIME_ROOT}/cpu"
                "$<TARGET_FILE_DIR:GarmentStyleMatch>/onnxruntime/cpu"
        COMMENT "Copying ONNX Runtime CPU binaries"
        VERBATIM
    )
endif()

if(GSM_MODELS_AVAILABLE)
    if(APPLE)
        set(_gsm_build_models_dir "$<TARGET_BUNDLE_DIR:GarmentStyleMatch>/Contents/Resources/models")
    else()
        set(_gsm_build_models_dir "$<TARGET_FILE_DIR:GarmentStyleMatch>/models")
    endif()
    add_custom_command(TARGET GarmentStyleMatch POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${GSM_MODEL_ROOT}"
                "${_gsm_build_models_dir}"
        COMMENT "Copying garment matching models"
        VERBATIM
    )
endif()

set_target_properties(GarmentStyleMatch PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER  com.garmentstylematch.app
    MACOSX_BUNDLE_BUNDLE_VERSION  ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
)

install(TARGETS GarmentStyleMatch
    RUNTIME DESTINATION bin
    BUNDLE  DESTINATION bin
)

if(APPLE)
    set(_gsm_runtime_install_dir "bin/GarmentStyleMatch.app/Contents/MacOS")
    set(_gsm_models_install_dir "bin/GarmentStyleMatch.app/Contents/Resources/models")
else()
    set(_gsm_runtime_install_dir "bin")
    set(_gsm_models_install_dir "bin/models")
endif()
install(DIRECTORY "${GSM_ONNXRUNTIME_RUNTIME_ROOT}/"
    DESTINATION "${_gsm_runtime_install_dir}/onnxruntime"
)
if(WIN32)
    install(DIRECTORY "${GSM_WINDOWSML_RUNTIME_ROOT}/"
        DESTINATION "${_gsm_runtime_install_dir}/onnxruntime/windowsml"
    )
    install(FILES "${GSM_WINDOWSML_RUNTIME_ROOT}/Microsoft.Windows.AI.MachineLearning.dll"
        DESTINATION "${_gsm_runtime_install_dir}"
    )
endif()
if(GSM_MODELS_AVAILABLE)
    install(DIRECTORY "${GSM_MODEL_ROOT}/"
        DESTINATION "${_gsm_models_install_dir}"
    )
endif()
install(PROGRAMS "${CMAKE_SOURCE_DIR}/scripts/download-models.ps1"
    DESTINATION bin/scripts
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

# Keep the complete portable installation under bin/. Qt's deployment support
# otherwise places these directories next to bin/ at the install prefix.
if(APPLE)
    install(CODE "
        set(_gsm_install_prefix \"\${CMAKE_INSTALL_PREFIX}\")
        set(CMAKE_INSTALL_PREFIX \"\${CMAKE_INSTALL_PREFIX}/bin\")
        set(QT_DEPLOY_PREFIX \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}\")
        include(\"${GSM_DEPLOY_SCRIPT}\")
        set(CMAKE_INSTALL_PREFIX \"\${_gsm_install_prefix}\")
        unset(QT_DEPLOY_PREFIX)
        unset(_gsm_install_prefix)
    ")
else()
    install(CODE "
        set(QT_DEPLOY_LIBEXEC_DIR \"bin/libexec\")
        set(QT_DEPLOY_LIB_DIR \"bin/lib\")
        set(QT_DEPLOY_PLUGINS_DIR \"bin/plugins\")
        set(QT_DEPLOY_QML_DIR \"bin/qml\")
        set(QT_DEPLOY_TRANSLATIONS_DIR \"bin/translations\")
        include(\"${GSM_DEPLOY_SCRIPT}\")
    ")
endif()
