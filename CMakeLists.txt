# ==============================================================================
# GRPD - Top-Level CMakeLists (Global Build Engine)
# Architecture: Pure dispatcher, contains no business dependencies,
#               strictly follows module autonomy principles.
# ==============================================================================

# 1. Set minimum CMake version (4.0.3 introduced improved FetchContent support)
cmake_minimum_required(VERSION 3.10)

# 2. Define project name, version, and language (C++)
project(GRPD VERSION 1.0.0 LANGUAGES CXX)

# ------------------------------------------------------------------------------
# Global compilation standards and policies
# ------------------------------------------------------------------------------
# Enforce C++17 standard (for std::filesystem, std::unique_ptr, etc.)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Disable compiler-specific extensions (e.g. gnu++17) for maximum cross-platform portability
set(CMAKE_CXX_EXTENSIONS OFF)
# Export compile_commands.json for IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/release")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/bin/debug")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/lib/release")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/lib/debug")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/lib/release")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/lib/debug")

# 确保输出目录在配置阶段被创建（MinGW 等单配置生成器不会自动创建 _RELEASE/_DEBUG 目录）
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/release")
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/debug")
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/release")
file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/lib/debug")

# ------------------------------------------------------------------------------
# Core: Delegate control to top-level modules
# ------------------------------------------------------------------------------
# Include the base library module (data structures, IO, constitutive models, etc.)
add_subdirectory(PDCommon)

# Include the main solver engine module (time integration, main entry point, etc.)
add_subdirectory(Src)

# Include third-party dependencies
add_subdirectory(Third)
# ------------------------------------------------------------------------------
# Build status output
# ------------------------------------------------------------------------------
message(STATUS "==================================================")
message(STATUS "GRPD Top-Level Configuration Done!")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Architecture: Decoupled (PDCommon + Src + Third)")
message(STATUS "==================================================")
