# ==============================================================================
# PDCommon/Model/CMakeLists.txt
# ==============================================================================

# 1. Define object library (produces .obj, not .lib)
# Manually list source files for CMake 4.0+ stability
add_library(Model_Obj OBJECT 
    src/Particle.cpp
    src/ParticleManager.cpp
)

# 2. Add header files to target
target_sources(Model_Obj PRIVATE
    include/Particle.h
    include/ParticleManager.h
)

# 3. Set header include paths for this module
# PUBLIC ensures targets linking to Model_Obj can see these headers
target_include_directories(Model_Obj PUBLIC include)

target_link_libraries(Model_Obj PRIVATE yaml-cpp OpenMP::OpenMP_CXX Utils_Obj Material_Obj)