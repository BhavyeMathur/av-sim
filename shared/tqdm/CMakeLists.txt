file(GLOB SRC_FILES ./*.cpp)
add_library(tqdm STATIC ${SRC_FILES})

target_include_directories(tqdm PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
