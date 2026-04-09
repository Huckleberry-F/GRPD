# ==============================================================================
# PDCommon/Fracture/CMakeLists.txt
# ==============================================================================

add_library(Fracture_Obj OBJECT 
    src/FractureRegistry.cpp
    src/FractureModel.cpp
    src/BondStretchFracture.cpp
    src/QuadCrack.cpp
    src/DamageValueFracture.cpp
    src/EqPSFracture.cpp
)

target_sources(Fracture_Obj PRIVATE
    include/FractureRegistry.h
    include/FractureModel.h
    include/BondStretchFracture.h
    include/PreCrackModel.h
    include/PreCrackRegistry.h
    include/QuadCrack.h
    include/DamageValueFracture.h
    include/EqPSFracture.h
)

target_include_directories(Fracture_Obj PUBLIC include)

target_link_libraries(Fracture_Obj PRIVATE yaml-cpp OpenMP::OpenMP_CXX Utils_Obj Core_Obj Neighbor_Obj Field_Obj Model_Obj)
