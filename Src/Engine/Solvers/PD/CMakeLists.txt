# ==============================================================================
# Src/Engine/Solvers/PD/CMakeLists.txt - PD 引擎模块
# ==============================================================================

add_library(PDSolve_Obj OBJECT
    PDEngine.cpp
    Pre/src/InitModel.cpp
    Pre/src/InitMaterial.cpp
    Pre/src/InitFields.cpp
    Pre/src/InitConditions.cpp
    Pre/src/InitNeighbors.cpp
    Pre/src/InitContact.cpp
    Pre/src/InitFractureModels.cpp
    Pre/src/InitPreCracks.cpp
    Pre/src/InitSolverComponents.cpp
    Pre/src/InitPostProcessing.cpp
    Run/src/PDEngineRun.cpp
    Post/src/PDEnginePost.cpp
)

target_sources(PDSolve_Obj PRIVATE
    PDEngine.h
    Pre/include/PDEnginePre.h
    Run/include/PDEngineRun.h
    Post/include/PDEnginePost.h
)

target_include_directories(PDSolve_Obj PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/Src/Engine/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Pre/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Run/include
    ${CMAKE_CURRENT_SOURCE_DIR}/Post/include
    ${PROJECT_SOURCE_DIR}/Src/Integration/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Utils/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Core/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Model/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Material/include
    ${PROJECT_SOURCE_DIR}/PDCommon/BC/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Field/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Neighbor/include
    ${PROJECT_SOURCE_DIR}/PDCommon/IO/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Kernel/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Fracture/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Contact/include
    ${PROJECT_SOURCE_DIR}/PDCommon/PostProcessing/include
)

target_link_libraries(PDSolve_Obj PRIVATE PDCommon Integration_Obj)
