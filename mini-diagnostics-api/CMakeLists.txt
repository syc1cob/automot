cmake_minimum_required(VERSION 3.22)
project(mini_diagnostics_api VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(OATPP_SOURCE_DIR "" CACHE PATH "Path to an existing oat++ 1.3.x source directory")

if(OATPP_SOURCE_DIR)
    set(OATPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(OATPP_LINK_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(OATPP_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory("${OATPP_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/oatpp")
    set(OATPP_TARGET oatpp)
else()
    find_package(oatpp 1.3 REQUIRED)
    set(OATPP_TARGET oatpp::oatpp)
endif()

add_executable(mini-diagnostics-api src/main.cpp)
target_include_directories(mini-diagnostics-api PRIVATE include)
target_link_libraries(mini-diagnostics-api PRIVATE ${OATPP_TARGET})

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(
        NAME mini-diagnostics-api-integration
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tests/api_tests.py"
                "$<TARGET_FILE:mini-diagnostics-api>"
    )
    set_tests_properties(mini-diagnostics-api-integration PROPERTIES TIMEOUT 60)
endif()