# ==============================================================================
# PDCommon/Utils/CMakeLists.txt
# ==============================================================================

# 1. Define object library (produces .obj, not .lib)
# Manually list source files for CMake 4.0+ stability
add_library(Utils_Obj OBJECT 
    src/Logger.cpp
)

# 2. Add header files to target
target_sources(Utils_Obj PRIVATE
    include/Logger.h
    include/DataExtractor.h
    # Other headers...
)
# 3. Set header include paths for this module
# PUBLIC ensures targets linking to Utils_Obj can see these headers
target_include_directories(Utils_Obj PUBLIC include)

# 4. Link dependencies if this module uses yaml-cpp or OpenMP headers
# Note: For OBJECT libraries, this is mainly to propagate include paths
target_link_libraries(Utils_Obj PRIVATE yaml-cpp OpenMP::OpenMP_CXX)
