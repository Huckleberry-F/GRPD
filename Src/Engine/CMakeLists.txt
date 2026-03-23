# ==============================================================================
# Src/Engine/CMakeLists.txt - 通用框架层
# ==============================================================================

# 1. Define object library for the simulation engine framework
add_library(Engine_Obj OBJECT 
    src/Engine.cpp
    src/EngineManager.cpp
    src/EngineRegistry.cpp
)

# 2. Add header files to target
target_sources(Engine_Obj PRIVATE
    include/Engine.h
    include/EngineManager.h
    include/EngineRegistry.h
)

# 3. Include directories
target_include_directories(Engine_Obj PRIVATE
    include
    ${PROJECT_SOURCE_DIR}/PDCommon/Utils/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Material/include
    ${PROJECT_SOURCE_DIR}/PDCommon/BC/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Core/include
    ${PROJECT_SOURCE_DIR}/Src
)

# 4. Link dependencies
target_link_libraries(Engine_Obj PRIVATE PDCommon Integration_Obj)

# 5. Add specific solvers
add_subdirectory(Solvers/PD)
