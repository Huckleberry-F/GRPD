# ==============================================================================
# Src/Solve/PD/CMakeLists.txt - PD 求解器模块
# ==============================================================================

add_library(PDSolve_Obj OBJECT
    src/PDSolver.cpp
    src/PD_Model.cpp
    src/PD_Material.cpp
    src/PD_Field.cpp
    src/PD_Condition.cpp
    src/PD_Neighbor.cpp
    src/PD_Solve.cpp
    src/PD_Output.cpp
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
