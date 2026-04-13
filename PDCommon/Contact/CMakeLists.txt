# ==============================================================================
# PDCommon/Contact/CMakeLists.txt
# ==============================================================================

# 1. Define object library (produces .obj, not .lib)
add_library(Contact_Obj OBJECT 
    src/ContactManager.cpp
    src/ContactRegistry.cpp
    src/SurfaceDetector.cpp
    src/ContactSpatialGrid.cpp
    src/NodeNodeContact.cpp
    src/NodeSurfaceContact.cpp
    src/PenaltyContact.cpp
    src/NonlinearPenaltyContact.cpp
    src/ViscousPenaltyContact.cpp
    src/KinematicContact.cpp
    src/SillingContact.cpp
)

# 2. Add header files to target
target_sources(Contact_Obj PRIVATE
    include/IContactAlgorithm.h
    include/ContactSpatialGrid.h
    include/NodeNodeContact.h
    include/NodeSurfaceContact.h
    include/ContactManager.h
    include/ContactRegistry.h
    include/SurfaceDetector.h
    include/PenaltyContact.h
    include/NonlinearPenaltyContact.h
    include/ViscousPenaltyContact.h
    include/KinematicContact.h
    include/SillingContact.h
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
