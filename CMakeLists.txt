cmake_minimum_required(VERSION 3.17)
project(ftxui-clap-support CXX)

# Require C++17 for FTXUI compatibility
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required dependencies
find_package(PkgConfig)

# Try to find FTXUI using different methods
find_package(ftxui QUIET)
if(NOT ftxui_FOUND)
    # Fallback: look for FTXUI as a subdirectory or external project
    if(NOT FTXUI_SOURCE_DIR)
        message(FATAL_ERROR "FTXUI not found. Please set FTXUI_SOURCE_DIR to the FTXUI source directory, or ensure ftxui is installed and findable by CMake.")
    endif()
    
    # Add FTXUI as subdirectory if source is available
    if(EXISTS "${FTXUI_SOURCE_DIR}/CMakeLists.txt")
        add_subdirectory(${FTXUI_SOURCE_DIR} ftxui)
    else()
        message(FATAL_ERROR "FTXUI_SOURCE_DIR does not contain a valid FTXUI source tree")
    endif()
endif()

# Find CLAP headers (should be available in the workspace)
if(NOT CLAP_INCLUDE_DIR)
    set(CLAP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../clap/include")
endif()

if(NOT EXISTS "${CLAP_INCLUDE_DIR}/clap/clap.h")
    message(FATAL_ERROR "CLAP headers not found. Please set CLAP_INCLUDE_DIR to the CLAP include directory.")
endif()

# Define the library
add_library(${PROJECT_NAME} STATIC
    src/ftxui-clap-support.cpp
    src/embedded-terminal.cpp
)

# Include directories
target_include_directories(${PROJECT_NAME} 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CLAP_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        src
)

# Link against required libraries
target_link_libraries(${PROJECT_NAME} 
    PUBLIC 
        ftxui::screen
        ftxui::dom  
        ftxui::component
)

# Platform-specific configurations
if(APPLE)
    target_sources(${PROJECT_NAME} PRIVATE
        src/embedded-terminal-macos.mm
    )
    target_link_libraries(${PROJECT_NAME} 
        PUBLIC 
            "-framework CoreFoundation" 
            "-framework Cocoa"
            "-framework CoreGraphics"
            "-framework Metal"
            "-framework MetalKit"
            "-framework CoreText"
    )
    # Enable Objective-C++ for .mm files
    set_target_properties(${PROJECT_NAME} PROPERTIES
        COMPILE_FLAGS "-x objective-c++"
    )
    
elseif(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE
        src/embedded-terminal-windows.cpp
    )
    target_link_libraries(${PROJECT_NAME} 
        PUBLIC 
            d2d1
            dwrite
            d3d11
            gdi32 
            user32
            kernel32
    )
    
elseif(UNIX)
    target_sources(${PROJECT_NAME} PRIVATE
        src/embedded-terminal-linux.cpp
    )
    
    # Find X11 libraries
    find_package(X11 REQUIRED)
    target_link_libraries(${PROJECT_NAME} 
        PUBLIC 
            ${X11_LIBRARIES}
    )
    target_include_directories(${PROJECT_NAME} 
        PRIVATE 
            ${X11_INCLUDE_DIR}
    )
endif()

# Compiler-specific options
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Enable threading support
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PUBLIC Threads::Threads)

# Optional: Enable address sanitizer for debug builds
option(FTXUI_CLAP_ENABLE_ASAN "Enable AddressSanitizer for debug builds" OFF)
if(FTXUI_CLAP_ENABLE_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
    target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address)
endif()

# Installation configuration
include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    EXPORT ftxui-clap-support-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# Create a CMake package config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ftxui-clap-support-config-version.cmake"
    VERSION 1.0.0
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ftxui-clap-support-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ftxui-clap-support-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ftxui-clap-support
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ftxui-clap-support-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ftxui-clap-support-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ftxui-clap-support
)

# Export targets
install(EXPORT ftxui-clap-support-targets
    FILE ftxui-clap-support-targets.cmake
    NAMESPACE ftxui-clap-support::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ftxui-clap-support
)

export(EXPORT ftxui-clap-support-targets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/ftxui-clap-support-targets.cmake"
    NAMESPACE ftxui-clap-support::
)

# Add tests if requested
option(FTXUI_CLAP_BUILD_TESTS "Build tests for ftxui-clap-support" OFF)
if(FTXUI_CLAP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Add examples if requested  
option(FTXUI_CLAP_BUILD_EXAMPLES "Build examples for ftxui-clap-support" OFF)
if(FTXUI_CLAP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Print configuration summary
message(STATUS "")
message(STATUS "ftxui-clap-support configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Build tests: ${FTXUI_CLAP_BUILD_TESTS}")
message(STATUS "  Build examples: ${FTXUI_CLAP_BUILD_EXAMPLES}")
message(STATUS "  Enable ASAN: ${FTXUI_CLAP_ENABLE_ASAN}")
message(STATUS "")
