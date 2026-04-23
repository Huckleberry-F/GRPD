# ==============================================================================
# PDCommon/IO/CMakeLists.txt
# ==============================================================================

# 1. Define object library (produces .obj, not .lib)
# Manually list source files for CMake stability
add_library(IO_Obj OBJECT 
    src/IOManager.cpp
    src/Outputer.cpp
    src/Writer.cpp
    src/VtkWriter.cpp
    src/MeshData.cpp
    src/MeshReader.cpp
    src/ReaderRegistry.cpp
    src/ReaderManager.cpp
    src/GrpdMeshReader.cpp
    src/InpMeshReader.cpp
)

# 2. Add header files to target
target_sources(IO_Obj PRIVATE
    include/IOManager.h
    include/Outputer.h
    include/Writer.h
    include/VtkWriter.h
    include/MeshData.h
    include/MeshReader.h
    include/ReaderRegistry.h
    include/ReaderManager.h
    include/GrpdMeshReader.h
    include/InpMeshReader.h
)

# 3. Set header include paths for this module
# PUBLIC ensures targets linking to IO_Obj can see these headers
target_include_directories(IO_Obj PUBLIC include)

# 4. Link dependencies: yaml-cpp, OpenMP, Model_Obj, Utils_Obj, Core_Obj, BC_Obj, Neighbor_Obj
target_link_libraries(IO_Obj PUBLIC yaml-cpp OpenMP::OpenMP_CXX Model_Obj Field_Obj Utils_Obj Core_Obj BC_Obj Neighbor_Obj)
