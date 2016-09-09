cmake_minimum_required(VERSION 2.6.2)
project(kcpev CXX C)
set(CMAKE_VERBOSE_MAKEFILE ON)

include(internal_utils.cmake)
config_compiler_and_linker()

set(KCPEV_DIR ${PROJECT_SOURCE_DIR}/src)
set(TEST_DIR ${PROJECT_SOURCE_DIR}/tests)

# sources
aux_source_directory(${KCPEV_DIR} KCPEV_SOURCES) 
aux_source_directory(${TEST_DIR} KCPEV_TEST_SOURCES) 

# third party
include_directories(
    "${PROJECT_SOURCE_DIR}/third_party/uthash/src/"
    "${PROJECT_SOURCE_DIR}/third_party/googletest/googletest/include/"
)

link_directories("${PROJECT_SOURCE_DIR}/third_party/googletest/build/googlemock/gtest")

# build libkcpev
add_library(kcpev_static STATIC ${KCPEV_SOURCES})
target_link_libraries(kcpev_static ev uuid)

add_library(kcpev SHARED ${KCPEV_SOURCES})
target_link_libraries(kcpev_static ev uuid)

# build tests
include_directories( ${TEST_DIR} )
include_directories( ${KCPEV_DIR} )
foreach(testfile ${KCPEV_TEST_SOURCES})
    get_filename_component(testname ${testfile} NAME_WE)
    add_executable(${testname} ${testfile})
    target_link_libraries(${testname} kcpev ev uuid gtest) 
endforeach(testfile)

