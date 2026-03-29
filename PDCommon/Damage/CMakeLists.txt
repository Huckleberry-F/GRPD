# ==============================================================================
# PDCommon/Damage/CMakeLists.txt
# ==============================================================================

add_library(Damage_Obj OBJECT 
    src/DamageRegistry.cpp
    src/CriticalStretch.cpp
    src/QuadCrack.cpp
)

target_sources(Damage_Obj PRIVATE
    include/DamageRegistry.h
    include/DamageModel.h
    include/CriticalStretch.h
    include/PreCrackModel.h
    include/PreCrackRegistry.h
    include/QuadCrack.h
)

target_include_directories(Damage_Obj PUBLIC include)

target_link_libraries(Damage_Obj PRIVATE yaml-cpp OpenMP::OpenMP_CXX Utils_Obj Core_Obj Neighbor_Obj Field_Obj Model_Obj)
