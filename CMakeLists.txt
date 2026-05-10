cmake_minimum_required(VERSION 3.14)
project(sim)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS_RELEASE
    "-Ofast -march=native -mtune=native -ffast-math -funroll-loops -fstrict-aliasing -flto -DNDEBUG -fvectorize -fslp-vectorize -fomit-frame-pointer -finline-functions -fno-strict-overflow -fno-math-errno -ffinite-math-only -fno-trapping-math -fassociative-math -freciprocal-math -fno-signed-zeros -fno-plt -pthread"
)
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-flto -Ofast -ffast-math -Wl,-Ofast")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "-flto -Ofast -ffast-math -Wl,-Ofast")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

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

# ---------- profile-guided optimization ----------

option(SIM_PGO_GENERATE "Build with PGO instrumentation" OFF)
option(SIM_PGO_USE "Build using collected PGO profile" OFF)

set(SIM_PGO_DIR "${CMAKE_SOURCE_DIR}/pgo" CACHE PATH "PGO profile directory")
set(SIM_PGO_PROFILE "${SIM_PGO_DIR}/sim.profdata" CACHE FILEPATH "Merged Clang PGO profile")

if (SIM_PGO_GENERATE AND SIM_PGO_USE)
    message(FATAL_ERROR "SIM_PGO_GENERATE and SIM_PGO_USE cannot both be ON")
endif ()

if (SIM_PGO_GENERATE)
    file(MAKE_DIRECTORY "${SIM_PGO_DIR}")

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(sim PRIVATE $<$<CONFIG:Release>:-fprofile-instr-generate=${SIM_PGO_DIR}/sim-%p.profraw>
        )

        target_link_options(sim PRIVATE $<$<CONFIG:Release>:-fprofile-instr-generate=${SIM_PGO_DIR}/sim-%p.profraw>)

        execute_process(COMMAND xcrun --find llvm-profdata
                        OUTPUT_VARIABLE LLVM_PROFDATA_EXECUTABLE
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                        ERROR_QUIET)

        if (NOT LLVM_PROFDATA_EXECUTABLE OR NOT EXISTS "${LLVM_PROFDATA_EXECUTABLE}")
            find_program(LLVM_PROFDATA_EXECUTABLE NAMES llvm-profdata REQUIRED)
        endif ()

        message(STATUS "Using llvm-profdata: ${LLVM_PROFDATA_EXECUTABLE}")

        add_custom_target(pgo_merge
                          COMMAND ${LLVM_PROFDATA_EXECUTABLE} merge
                          -output=${SIM_PGO_PROFILE}
                          ${SIM_PGO_DIR}/*.profraw
                          WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                          COMMENT "Merging Clang PGO profiles"
        )
    else ()
        message(FATAL_ERROR "PGO generate is only configured for Clang/GCC")
    endif ()

    message(STATUS "PGO generate enabled: profiles will be written to ${SIM_PGO_DIR}")
endif ()

if (SIM_PGO_USE)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        if (NOT EXISTS "${SIM_PGO_PROFILE}")
            message(FATAL_ERROR "Missing Clang PGO profile: ${SIM_PGO_PROFILE}")
        endif ()

        target_compile_options(sim PRIVATE -fprofile-instr-use=${SIM_PGO_PROFILE})
        target_link_options(sim PRIVATE $<$<CONFIG:Release>:-fprofile-instr-use=${SIM_PGO_PROFILE}>)
    else ()
        message(FATAL_ERROR "PGO use is only configured for Clang/GCC")
    endif ()

    message(STATUS "PGO use enabled")
endif ()

# ---------- profile-guided optimization ----------

set_target_properties(sim PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}")
target_include_directories(sim PUBLIC src)
target_include_directories(sim PUBLIC src/util)
target_include_directories(sim PUBLIC src/dsa)

# Uber H3
fetchcontent_declare(h3
                     GIT_REPOSITORY https://github.com/uber/h3.git
                     GIT_TAG v4.4.1
                     UPDATE_DISCONNECTED TRUE)
fetchcontent_makeavailable(h3)

target_include_directories(sim PRIVATE
                           ${h3_SOURCE_DIR}/src
                           ${h3_SOURCE_DIR}/src/h3lib/include)

# YAML
fetchcontent_declare(yaml-cpp
                     GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
                     GIT_TAG yaml-cpp-0.9.0)
fetchcontent_makeavailable(yaml-cpp)

# SQLite
find_package(SQLite3 REQUIRED)

# faster data structures
find_package(absl CONFIG REQUIRED)

# other sources
add_subdirectory(src/util/pandas)
add_subdirectory(src/util/tqdm)
target_link_libraries(sim PRIVATE pandas tqdm h3 yaml-cpp SQLite::SQLite3 absl::flat_hash_map)

target_compile_definitions(sim PRIVATE)
