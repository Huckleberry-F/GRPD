# ==============================================================================
# ThirdParty coordinator
# ==============================================================================

# Include locally extracted yaml-cpp
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/yaml-cpp/CMakeLists.txt")
    # Disable yaml-cpp tests and tools to speed up compilation
    set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    
    add_subdirectory(yaml-cpp)
else()
    message(FATAL_ERROR "yaml-cpp source not found in ThirdParty/yaml-cpp!")
endif()

# If you add Eigen or NetCDF in the future, also add_subdirectory here

# Include Eigen (header-only linear algebra library)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/eigen/CMakeLists.txt")
    # 为了避免 Eigen 庞大且复杂的 CMakeLists.txt 在 Windows 下进行大量的 try_compile 和编译器特征检测，
    # 导致配置时间大幅延长（特别是更新到 upstream master 后检测项更多），
    # 且因为项目仅需 Eigen 的头文件，我们采用 INTERFACE IMPORTED 方式声明 Eigen3::Eigen 目标，
    # 从而完全跳过 Eigen 的 add_subdirectory 配置，实现配置阶段的“零开销”。
    add_library(Eigen3::Eigen INTERFACE IMPORTED GLOBAL)
    set_target_properties(Eigen3::Eigen PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/eigen"
    )
else()
    message(FATAL_ERROR "Eigen source not found in Third/eigen!")
endif()