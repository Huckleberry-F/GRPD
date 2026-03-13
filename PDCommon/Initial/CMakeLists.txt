# ==============================================================================
# PDCommon/Initial/CMakeLists.txt
# ==============================================================================

# 1. Define object library (produces .obj, not .lib)
# Manually list source files for CMake 4.0+ stability
add_library(Initial_Obj OBJECT 
    src/Initial.cpp
    src/NeighborList.cpp
)

# 2. Add header files to target
target_sources(Initial_Obj PRIVATE
    include/NeighborList.h
)

# 3. Set header include paths for this module
# PUBLIC ensures targets linking to Initial_Obj can see these headers
target_include_directories(Initial_Obj PUBLIC include)

# 4. Link dependencies if this module uses yaml-cpp or OpenMP headers
# Note: For OBJECT libraries, this is mainly to propagate include paths
target_link_libraries(Initial_Obj PRIVATE yaml-cpp OpenMP::OpenMP_CXX Model_Obj Utils_Obj)