# ==============================================================================
# Src/Engine/Solvers/PD/CMakeLists.txt - PD 引擎模块
# ==============================================================================

add_library(PDSolve_Obj OBJECT
    src/PDEngine.cpp
    src/PDEngineInitializer.cpp
)

target_sources(PDSolve_Obj PRIVATE
    include/PDEngine.h
    include/PDEngineInitializer.h
)

target_include_directories(PDSolve_Obj PRIVATE
    include
    ${PROJECT_SOURCE_DIR}/Src/Engine/include
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
)

target_link_libraries(PDSolve_Obj PRIVATE PDCommon Integration_Obj)
