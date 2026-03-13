# ==============================================================================
# Src/Engine/CMakeLists.txt
# ==============================================================================

# 1. Define object library for the simulation engine
add_library(Engine_Obj OBJECT 
    src/SolverEngine.cpp
    src/SE_Model.cpp
    src/SE_Output.cpp
    src/SE_Material.cpp
    src/SE_Field.cpp
    src/SE_Condition.cpp
    src/SE_Solve.cpp
)

# 2. Add header files to target
target_sources(Engine_Obj PRIVATE
    include/SolverEngine.h
)

# 3. No public includes needed here, handled by parent Src/CMakeLists.txt
# but we do need the include paths to compile SolverEngine.cpp itself
target_include_directories(Engine_Obj PRIVATE
    include
    ${PROJECT_SOURCE_DIR}/PDCommon/Utils/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Material/include
    ${PROJECT_SOURCE_DIR}/PDCommon/BC/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Core/include
    ${PROJECT_SOURCE_DIR}/Src
)

# 4. Link dependencies (mostly for includes when using OBJECT library)
target_link_libraries(Engine_Obj PRIVATE PDCommon)
