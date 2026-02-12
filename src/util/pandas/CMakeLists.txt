find_package(Arrow REQUIRED)
find_package(Parquet REQUIRED)

file(GLOB SRC_FILES ./*.cpp)
add_library(pandas STATIC ${SRC_FILES})

target_link_libraries(pandas PUBLIC
                      Arrow::arrow_shared
                      Parquet::parquet_shared
)

#target_link_libraries(pandas PRIVATE Arrow::arrow_shared)
#target_link_libraries(pandas PRIVATE Parquet::parquet_shared)
#target_include_directories(pandas PRIVATE ${ARROW_INCLUDE_DIR})
