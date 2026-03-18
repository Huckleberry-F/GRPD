# ==============================================================================
# Src/Solve/PD/CMakeLists.txt - PD 求解器模块
# ==============================================================================

add_library(PDSolve_Obj OBJECT
    src/Initialize.cpp
    src/InitModel.cpp
    src/InitMaterial.cpp
    src/InitFields.cpp
    src/InitConditions.cpp
    src/InitNeighbors.cpp
    src/Solve.cpp
    src/Output.cpp
    src/NOSB_T.cpp
    src/ExplicitEuler.cpp
    src/InitSolverComponents.cpp
)

target_sources(PDSolve_Obj PRIVATE
    include/PDSolver.h
    include/PDKernel.h
    include/TimeIntegrator.h
    include/ExplicitEuler.h
    include/NOSB_T.h
)

target_include_directories(PDSolve_Obj PRIVATE
    include
    ${PROJECT_SOURCE_DIR}/Src/Engine/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Utils/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Core/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Model/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Material/include
    ${PROJECT_SOURCE_DIR}/PDCommon/BC/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Field/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Neighbor/include
    ${PROJECT_SOURCE_DIR}/PDCommon/IO/include
)

target_link_libraries(PDSolve_Obj PRIVATE PDCommon)
