# ==============================================================================
# PDCommon/Contact/CMakeLists.txt
# ==============================================================================

# 1. Define object library (produces .obj, not .lib)
add_library(Contact_Obj OBJECT 
    src/ContactManager.cpp
    src/ContactRegistry.cpp
    src/SurfaceDetector.cpp
    src/ContactSpatialGrid.cpp
    
    # --- 扁平化顶层接触算法 (Types) ---
    src/GeneralContact.cpp
    src/NTNEvaluator.cpp
    src/NTCEvaluator.cpp
    src/NTSEvaluator.cpp
    
    # --- 物理力学定律 (ForceLaws & FrictionLaws) ---
    src/PenaltyForceLaw.cpp
    src/ViscousForceLaw.cpp
    src/NonlinearPenaltyForceLaw.cpp
    src/SillingForceLaw.cpp
    src/KinematicForceLaw.cpp
    src/ExplicitImpulseFrictionLaw.cpp
    src/TanhFrictionLaw.cpp
    src/ElasticStickSlipLaw.cpp
)

# 2. Add header files to target
target_sources(Contact_Obj PRIVATE
    include/IContactAlgorithm.h
    include/ContactManager.h
    include/ContactRegistry.h
    include/SurfaceDetector.h
    include/ContactSpatialGrid.h
    
    # --- 扁平化顶层接触算法 ---
    include/GeneralContact.h
    include/IContactEvaluator.h
    include/NTNEvaluator.h
    include/NTCEvaluator.h
    
    # --- 物理力学定律 ---
    include/IContactForceLaw.h
    include/PenaltyForceLaw.h
    include/ViscousForceLaw.h
    include/NonlinearPenaltyForceLaw.h
    include/SillingForceLaw.h
    include/KinematicForceLaw.h
    include/IFrictionLaw.h
    include/ExplicitImpulseFrictionLaw.h
    include/TanhFrictionLaw.h
    include/ElasticStickSlipLaw.h
)

# 3. Set header include paths for this module
target_include_directories(Contact_Obj PUBLIC include)
target_include_directories(Contact_Obj PRIVATE 
    ${PROJECT_SOURCE_DIR}/PDCommon/Core/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Material/include
    ${PROJECT_SOURCE_DIR}/PDCommon/BC/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Neighbor/include
)

# 4. Link dependencies
target_link_libraries(Contact_Obj PUBLIC yaml-cpp Field_Obj Model_Obj Neighbor_Obj OpenMP::OpenMP_CXX Eigen3::Eigen)
