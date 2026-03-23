# ==============================================================================
# PDCommon/Neighbor/CMakeLists.txt - 邻域搜索与接触搜索模块
# ==============================================================================

# 1. Define object library
add_library(Neighbor_Obj OBJECT 
    src/NeighborList.cpp
)

# 2. Add header files to target
target_sources(Neighbor_Obj PRIVATE
    include/NeighborList.h
    include/ContactSearch.h
)

# 3. Set header include paths
target_include_directories(Neighbor_Obj PUBLIC include)

# 4. Link dependencies
target_link_libraries(Neighbor_Obj PRIVATE yaml-cpp OpenMP::OpenMP_CXX Model_Obj Utils_Obj)