find_package(LibArchive REQUIRED)

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

add_executable(MatchControllerTest
    MatchControllerTest.cpp
    ../src/core/MatchController.cpp
    ../src/core/MatchController.h
    ../src/core/CandidateListModel.cpp
    ../src/core/CandidateListModel.h
    ../src/core/GalleryListModel.cpp
    ../src/core/GalleryListModel.h
    ../src/core/PhotoListModel.cpp
    ../src/core/PhotoListModel.h
    ../src/core/PptPageListModel.cpp
    ../src/core/PptPageListModel.h
    ../src/core/PptStyleExtractor.cpp
    ../src/core/PptStyleExtractor.h
)

target_include_directories(MatchControllerTest PRIVATE
    ${CMAKE_SOURCE_DIR}/src/core
)

target_link_libraries(MatchControllerTest PRIVATE
    Qt6::Concurrent
    Qt6::Core
    Qt6::Gui
    Qt6::QuickControls2
    Qt6::Widgets
    LibArchive::LibArchive
    $<$<PLATFORM_ID:Windows>:Qt6::AxContainer>
)

add_test(NAME MatchControllerTest COMMAND MatchControllerTest)
