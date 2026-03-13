# ==============================================================================
# PDCommon/Material/CMakeLists.txt
# ==============================================================================

# 1. Define object library (produces .obj, not .lib)
# Manually list source files for CMake 4.0+ stability
add_library(Material_Obj OBJECT 
    src/Material.cpp
    src/MaterialManager.cpp
    src/MaterialRegistry.cpp
    src/ThermalMat.cpp
)

# 2. Add header files to target
target_sources(Material_Obj PRIVATE
    include/Material.h
    include/MaterialManager.h
    include/MaterialRegistry.h
    include/ThermalMat.h
)

# 3. Set header include paths for this module
# PUBLIC ensures targets linking to Material_Obj can see these headers
target_include_directories(Material_Obj PUBLIC include)

# 4. Link dependencies if this module uses yaml-cpp or OpenMP headers
# Note: For OBJECT libraries, this is mainly to propagate include paths
target_link_libraries(Material_Obj PRIVATE yaml-cpp OpenMP::OpenMP_CXX Utils_Obj Field_Obj)