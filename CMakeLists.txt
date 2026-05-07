cmake_minimum_required(VERSION 3.14)
project(sim)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS_RELEASE
    "-Ofast -march=native -mtune=native -ffast-math -funroll-loops -fstrict-aliasing -DNDEBUG -fvectorize -fslp-vectorize -fno-strict-overflow -fno-math-errno -ffinite-math-only -fno-trapping-math -fassociative-math -freciprocal-math -fno-signed-zeros -fno-plt -pthread"
)

set(CMAKE_EXE_LINKER_FLAGS_RELEASE
    "-Ofast -ffast-math -Wl,-Ofast"
)

set(CMAKE_SHARED_LINKER_FLAGS_RELEASE
    "-Ofast -ffast-math -Wl,-Ofast"
)

message(STATUS "Release CXX flags: ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "Release linker flags: ${CMAKE_EXE_LINKER_FLAGS_RELEASE}")

include(FetchContent)

if (POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif ()

file(GLOB_RECURSE SIM_SOURCES "src/*.cpp")
add_executable(sim ${SIM_SOURCES})

set_target_properties(sim PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")
target_include_directories(sim PUBLIC src)
target_include_directories(sim PUBLIC src/util)
target_include_directories(sim PUBLIC src/dsa)

# Uber H3
fetchcontent_declare(
        h3
        GIT_REPOSITORY https://github.com/uber/h3.git
        GIT_TAG v4.4.1
        UPDATE_DISCONNECTED TRUE
)
fetchcontent_makeavailable(h3)

target_include_directories(sim PRIVATE
                           ${h3_SOURCE_DIR}/src
                           ${h3_SOURCE_DIR}/src/h3lib/include
)

# YAML
fetchcontent_declare(
        yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG yaml-cpp-0.9.0
)
fetchcontent_makeavailable(yaml-cpp)

# SQLite
find_package(SQLite3 REQUIRED)

# other sources
add_subdirectory(src/util/pandas)
add_subdirectory(src/util/tqdm)
target_link_libraries(sim PRIVATE pandas tqdm h3 yaml-cpp SQLite::SQLite3)

target_compile_definitions(sim PRIVATE)
