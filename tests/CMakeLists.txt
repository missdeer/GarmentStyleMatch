if(WIN32)
    add_executable(PptStyleExtractorTest
        PptStyleExtractorTest.cpp
        ../src/core/PptStyleExtractor.cpp
        ../src/core/PptStyleExtractor.h
    )

    target_include_directories(PptStyleExtractorTest PRIVATE
        ${CMAKE_SOURCE_DIR}/src/core
    )

    target_link_libraries(PptStyleExtractorTest PRIVATE
        Qt6::Core
        LibArchive::LibArchive
    )

    add_test(NAME PptStyleExtractorTest COMMAND PptStyleExtractorTest)
endif()

add_executable(GalleryListModelTest
    GalleryListModelTest.cpp
    ../src/core/GalleryListModel.cpp
    ../src/core/GalleryListModel.h
)

target_include_directories(GalleryListModelTest PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
)

target_link_libraries(GalleryListModelTest PRIVATE
    Qt6::Core
)

add_test(NAME GalleryListModelTest COMMAND GalleryListModelTest)

if(WIN32)
    add_executable(PptPageListModelTest
        PptPageListModelTest.cpp
        ../src/core/PptPageListModel.cpp
        ../src/core/PptPageListModel.h
    )

    target_include_directories(PptPageListModelTest PRIVATE
        ${CMAKE_SOURCE_DIR}/src/core
    )

    target_link_libraries(PptPageListModelTest PRIVATE
        Qt6::Core
    )

    add_test(NAME PptPageListModelTest COMMAND PptPageListModelTest)
endif()

add_executable(ImageMetadataTest
    ImageMetadataTest.cpp
    ../src/core/ImageMetadata.cpp
    ../src/core/ImageMetadata.h
)

target_include_directories(ImageMetadataTest PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
)

target_link_libraries(ImageMetadataTest PRIVATE
    Qt6::Core
    Qt6::Gui
)

add_test(NAME ImageMetadataTest COMMAND ImageMetadataTest)

add_executable(MatchResultStoreTest
    MatchResultStoreTest.cpp
    ../src/core/MatchResultStore.cpp
    ../src/core/MatchResultStore.h
    ../src/core/SQLiteDB.cpp
    ../src/core/SQLiteDB.h
    ../src/core/SQLiteStatement.cpp
    ../src/core/SQLiteStatement.h
)

target_include_directories(MatchResultStoreTest PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
)

target_link_libraries(MatchResultStoreTest PRIVATE
    Qt6::Core
    gsm_sqlite
)

add_test(NAME MatchResultStoreTest COMMAND MatchResultStoreTest)

add_executable(HttpDownloaderTest
    HttpDownloaderTest.cpp
    ../src/core/HttpDownloader.cpp
    ../src/core/HttpDownloader.h
)

target_include_directories(HttpDownloaderTest PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
)

target_link_libraries(HttpDownloaderTest PRIVATE
    Qt6::Core
    Qt6::Network
)

add_test(NAME HttpDownloaderTest COMMAND HttpDownloaderTest)

add_executable(LuaCategoryRuleEngineTest
    LuaCategoryRuleEngineTest.cpp
    ../src/core/LuaCategoryRuleEngine.cpp
    ../src/core/LuaCategoryRuleEngine.h
)

# Keep the test objects synchronized when localized /showIncludes output cannot
# provide a dependable header depfile.
set_property(SOURCE LuaCategoryRuleEngineTest.cpp ../src/core/LuaCategoryRuleEngine.cpp APPEND PROPERTY
    OBJECT_DEPENDS
        "${CMAKE_SOURCE_DIR}/src/core/LuaCategoryRuleEngine.h"
        "${CMAKE_SOURCE_DIR}/resources/category-rules/current-brand.lua"
)

target_include_directories(LuaCategoryRuleEngineTest PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
)

target_link_libraries(LuaCategoryRuleEngineTest PRIVATE
    Qt6::Core
    gsm_lua
)

add_test(NAME LuaCategoryRuleEngineTest COMMAND LuaCategoryRuleEngineTest
    "${CMAKE_SOURCE_DIR}/resources/category-rules/current-brand.lua"
)

set(_gsm_match_controller_test_sources
    MatchControllerTest.cpp
    ../src/core/MatchController.cpp
    ../src/core/MatchController.h
    ../src/core/HttpDownloader.cpp
    ../src/core/HttpDownloader.h
    ../src/core/GarmentMatcher.cpp
    ../src/core/GarmentMatcher.h
    ../src/core/CandidateListModel.cpp
    ../src/core/CandidateListModel.h
    ../src/core/GalleryListModel.cpp
    ../src/core/GalleryListModel.h
    ../src/core/PhotoListModel.cpp
    ../src/core/PhotoListModel.h
    ../src/core/MatchResultStore.cpp
    ../src/core/MatchResultStore.h
    ../src/core/SQLiteDB.cpp
    ../src/core/SQLiteDB.h
    ../src/core/SQLiteStatement.cpp
    ../src/core/SQLiteStatement.h
)
if(WIN32)
    list(APPEND _gsm_match_controller_test_sources
        ../src/core/PptPageListModel.cpp
        ../src/core/PptPageListModel.h
        ../src/core/PptStyleExtractor.cpp
        ../src/core/PptStyleExtractor.h
        ../src/core/WindowsMlExecutionProvider.cpp
        ../src/core/WindowsMlExecutionProvider.h
    )
endif()
add_executable(MatchControllerTest ${_gsm_match_controller_test_sources})

# Keep the test object's MatchController layout synchronized even when localized
# MSVC /showIncludes output prevents Ninja from discovering header dependencies.
set_property(SOURCE MatchControllerTest.cpp APPEND PROPERTY
    OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/src/core/MatchController.h"
)
set_property(SOURCE ../src/core/MatchController.cpp APPEND PROPERTY
    OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/src/core/HttpDownloader.h"
)

target_include_directories(MatchControllerTest PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
)

target_link_libraries(MatchControllerTest PRIVATE
    Qt6::Concurrent
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::QuickControls2
    Qt6::Widgets
    LibArchive::LibArchive
    gsm_onnxruntime_headers
    ${OpenCV_LIBS}
    gsm_sqlite
    gsm_windowsml_api
    $<$<PLATFORM_ID:Windows>:Qt6::AxContainer>
)

add_test(NAME MatchControllerTest COMMAND MatchControllerTest)

if(GSM_MODELS_AVAILABLE)
    set(_gsm_runtime_test_sources
        GarmentMatcherRuntimeTest.cpp
        ../src/core/GarmentMatcher.cpp
        ../src/core/GarmentMatcher.h
        ../src/core/SQLiteDB.cpp
        ../src/core/SQLiteDB.h
        ../src/core/SQLiteStatement.cpp
        ../src/core/SQLiteStatement.h
    )
    if(WIN32)
        list(APPEND _gsm_runtime_test_sources
            ../src/core/WindowsMlExecutionProvider.cpp
            ../src/core/WindowsMlExecutionProvider.h
        )
    endif()
    add_executable(GarmentMatcherRuntimeTest ${_gsm_runtime_test_sources})

    target_include_directories(GarmentMatcherRuntimeTest PRIVATE
        ${CMAKE_SOURCE_DIR}/src/core
    )

    target_link_libraries(GarmentMatcherRuntimeTest PRIVATE
        Qt6::Core
        Qt6::Gui
        gsm_onnxruntime_headers
        ${OpenCV_LIBS}
        gsm_sqlite
        gsm_windowsml_api
    )

    add_custom_command(TARGET GarmentMatcherRuntimeTest POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${GSM_MODEL_ROOT}"
                "$<TARGET_FILE_DIR:GarmentMatcherRuntimeTest>/models"
        COMMENT "Copying garment matching models for runtime tests"
        VERBATIM
    )
    if(WIN32)
        add_custom_command(TARGET GarmentMatcherRuntimeTest POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${GSM_ONNXRUNTIME_RUNTIME_ROOT}/cuda"
                    "$<TARGET_FILE_DIR:GarmentMatcherRuntimeTest>/onnxruntime/cuda"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${GSM_ONNXRUNTIME_RUNTIME_ROOT}/directml"
                    "$<TARGET_FILE_DIR:GarmentMatcherRuntimeTest>/onnxruntime/directml"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${GSM_WINDOWSML_RUNTIME_ROOT}"
                    "$<TARGET_FILE_DIR:GarmentMatcherRuntimeTest>/onnxruntime/windowsml"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${GSM_WINDOWSML_RUNTIME_ROOT}/Microsoft.Windows.AI.MachineLearning.dll"
                    "$<TARGET_FILE_DIR:GarmentMatcherRuntimeTest>"
            COMMENT "Copying Windows ONNX Runtime binaries for runtime tests"
            VERBATIM
        )
    else()
        add_custom_command(TARGET GarmentMatcherRuntimeTest POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${GSM_ONNXRUNTIME_RUNTIME_ROOT}/cpu"
                    "$<TARGET_FILE_DIR:GarmentMatcherRuntimeTest>/onnxruntime/cpu"
            COMMENT "Copying ONNX Runtime CPU binaries for runtime tests"
            VERBATIM
        )
    endif()

    add_test(NAME GarmentMatcherRuntimeTest COMMAND GarmentMatcherRuntimeTest)
    if(APPLE)
        add_test(NAME GarmentMatcherCoreMLRuntimeTest COMMAND GarmentMatcherRuntimeTest coreml)
    endif()
    if(WIN32)
        add_test(NAME GarmentMatcherDirectMLRuntimeTest COMMAND GarmentMatcherRuntimeTest directml)
        add_test(NAME GarmentMatcherWindowsMLCpuRuntimeTest COMMAND GarmentMatcherRuntimeTest "windows ml · cpu")
        add_test(NAME GarmentMatcherWindowsMLDirectMLRuntimeTest COMMAND GarmentMatcherRuntimeTest "windows ml · directml")
        find_file(GSM_TENSORRT_NVINFER_DLL nvinfer_10.dll HINTS "$ENV{TENSORRT_ROOT}/lib")
        find_file(GSM_TENSORRT_PARSER_DLL nvonnxparser_10.dll HINTS "$ENV{TENSORRT_ROOT}/lib")
        if(EXISTS "${GSM_ONNXRUNTIME_RUNTIME_ROOT}/cuda/onnxruntime_providers_tensorrt.dll"
           AND GSM_TENSORRT_NVINFER_DLL
           AND GSM_TENSORRT_PARSER_DLL)
            add_test(NAME GarmentMatcherTensorRTRuntimeTest COMMAND GarmentMatcherRuntimeTest tensorrt)
        endif()
        add_test(NAME GarmentMatcherProviderRestartRuleTest COMMAND GarmentMatcherRuntimeTest restart-rule)
    endif()
endif()
