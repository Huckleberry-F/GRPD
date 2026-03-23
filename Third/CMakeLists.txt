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
    # 禁用 Eigen 的测试和文档构建，加速编译
    set(EIGEN_BUILD_DOC OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(EIGEN_BUILD_PKGCONFIG OFF CACHE BOOL "" FORCE)
    add_subdirectory(eigen)
else()
    message(FATAL_ERROR "Eigen source not found in Third/eigen!")
endif()