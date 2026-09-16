cmake_minimum_required(VERSION 3.14)

project(LibTFminiPlus C)

add_library(TFminiPlus STATIC Src/TFminiPlus.c)
target_include_directories(TFminiPlus PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Inc>
	$<INSTALL_INTERFACE:include>
)
set_target_properties(TFminiPlus PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	target_compile_options(TFminiPlus PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(GNUInstallDirs)
install(TARGETS TFminiPlus
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES Inc/TFminiPlus.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
