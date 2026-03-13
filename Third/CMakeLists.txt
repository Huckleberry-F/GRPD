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