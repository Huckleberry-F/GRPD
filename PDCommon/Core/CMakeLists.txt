# ==============================================================================
# PDCommon/Core/CMakeLists.txt
# ==============================================================================

# 1. Define object library for the simulation hub
add_library(Core_Obj OBJECT 
    src/PDContext.cpp
)

# 2. Add header files to target
target_sources(Core_Obj PRIVATE
    include/PDContext.h
)

# 3. Set header include paths for this module
target_include_directories(Core_Obj PUBLIC include)

# 4. Link dependencies: PUBLIC because headers include these modules
target_link_libraries(Core_Obj PUBLIC Model_Obj Material_Obj Field_Obj Utils_Obj Initial_Obj BC_Obj yaml-cpp)
