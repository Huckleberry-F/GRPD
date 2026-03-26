# ==============================================================================
# PDCommon/BC/CMakeLists.txt
# ==============================================================================

# 1. Define object library (produces .obj, not .lib)
# Manually list source files for CMake 4.0+ stability
add_library(BC_Obj OBJECT 
    src/BC.cpp
    src/ThermalBC.cpp
    src/MechanicalBC.cpp
    src/BCManager.cpp
    src/BCRegistry.cpp
)
# 2. Add header files to target
target_sources(BC_Obj PRIVATE
    include/BC.h
    include/ThermalBC.h
    include/MechanicalBC.h
    include/BCManager.h
    include/BCRegistry.h
)
# 3. Set header include paths for this module
# PUBLIC ensures targets linking to BC_Obj can see these headers
target_include_directories(BC_Obj PUBLIC include)

# 4. Link dependencies: PUBLIC because ThermalBC.h includes ThermalField.h
target_link_libraries(BC_Obj PUBLIC Field_Obj Utils_Obj yaml-cpp OpenMP::OpenMP_CXX)