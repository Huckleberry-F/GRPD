# ==============================================================================
# PDCommon/IO/CMakeLists.txt
# ==============================================================================

# 1. Define object library (produces .obj, not .lib)
# Manually list source files for CMake 4.0+ stability
add_library(IO_Obj OBJECT 
    src/GrpdReader.cpp
    src/Writer.cpp
    src/VtkWriter.cpp
)

# 2. Add header files to target
target_sources(IO_Obj PRIVATE
    include/GrpdReader.h
    include/Writer.h
    include/VtkWriter.h
    # Other headers...
)

# 3. Set header include paths for this module
# PUBLIC ensures targets linking to IO_Obj can see these headers
target_include_directories(IO_Obj PUBLIC include)

# 4. Link dependencies: yaml-cpp, OpenMP, Model_Obj, Utils_Obj, Core_Obj, BC_Obj, Initial_Obj
target_link_libraries(IO_Obj PUBLIC yaml-cpp OpenMP::OpenMP_CXX Model_Obj Utils_Obj Core_Obj BC_Obj Initial_Obj)