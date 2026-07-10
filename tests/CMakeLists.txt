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
