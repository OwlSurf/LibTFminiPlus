cmake_minimum_required(VERSION 3.14)

project(TestLibTFminiPlus)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD 99)

include(FetchContent)
FetchContent_Declare(
	googletest
	URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_library(TFminiPlus STATIC ../Src/TFminiPlus.c)
target_include_directories(TFminiPlus PUBLIC ../Inc)

enable_testing()
add_executable(runtest test.cpp)
target_link_libraries(runtest PRIVATE TFminiPlus gtest_main)

include(GoogleTest)
gtest_discover_tests(runtest)
