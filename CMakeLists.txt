cmake_minimum_required(VERSION 2.6.2)
project(kcpev CXX C)
set(CMAKE_VERBOSE_MAKEFILE ON)

include(internal_utils.cmake)
config_compiler_and_linker()

message("cxx flags ${CMAKE_CXX_FLAGS}")
message("c flags ${CMAKE_C_FLAGS}")

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

if(MSVC)
    include_directories(
    "${PROJECT_SOURCE_DIR}/third_party/libev-win")
    link_directories(
    "${PROJECT_SOURCE_DIR}/third_party/googletest/build/googlemock/gtest/$(ConfigurationName)/"
    "${PROJECT_SOURCE_DIR}/third_party/libev-win/build/$(ConfigurationName)/"
    "${PROJECT_SOURCE_DIR}/build/$(ConfigurationName)/"
    )
endif()

# build libkcpev
add_library(kcpev_static STATIC ${KCPEV_SOURCES})
add_library(kcpev SHARED ${KCPEV_SOURCES})

if(MSVC) 
    target_link_libraries(kcpev_static libev_static ws2_32)
    target_link_libraries(kcpev libev_static ws2_32)
else()
    target_link_libraries(kcpev_static ev uuid)
    target_link_libraries(kcpev ev uuid)
endif()

set(MSVC_NOT_BUILD "client_kcp_simple_test ")

list(REMOVE_ITEM KCPEV_TEST_SOURCES "${PROJECT_SOURCE_DIR}/tests/test.cc")

if(MSVC)
    list(REMOVE_ITEM KCPEV_TEST_SOURCES 
        "${PROJECT_SOURCE_DIR}/tests/kcpev_send_test.cpp"
        "${PROJECT_SOURCE_DIR}/tests/kcpev_package_test.cpp")
#    add_custom_command(TARGET kcpev POST_BUILD
#        COMMAND copy "${PROJECT_SOURCE_DIR}\\third_party\\googletest\\build\\googlemock\\gtest\\$(ConfigurationName)\\gtest.dll"  "${PROJECT_SOURCE_DIR}\\build\\$(ConfigurationName)"
#    )
endif()

# build tests
include_directories( ${TEST_DIR} )
include_directories( ${KCPEV_DIR} )
foreach(testfile ${KCPEV_TEST_SOURCES})
    get_filename_component(testname ${testfile} NAME_WE)
    add_executable(${testname} ${testfile} "${PROJECT_SOURCE_DIR}/tests/test.cc")
    if(MSVC)
        target_link_libraries(${testname} kcpev_static libev_static gtest ws2_32) 
    else()
        target_link_libraries(${testname} kcpev_static libev_static gtest) 
    endif()
endforeach(testfile)

