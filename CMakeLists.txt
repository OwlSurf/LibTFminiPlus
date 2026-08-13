cmake_minimum_required(VERSION 3.14)

project(LibTFminiPlus C)

add_library(TFminiPlus STATIC Src/TFminiPlus.c)
target_include_directories(TFminiPlus PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Inc>
	$<INSTALL_INTERFACE:include>
)
set_target_properties(TFminiPlus PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
