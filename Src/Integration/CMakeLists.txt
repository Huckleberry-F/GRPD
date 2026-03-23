# ==============================================================================
# Src/Integration/CMakeLists.txt
# ==============================================================================

add_library(Integration_Obj OBJECT 
    src/ExplicitEuler.cpp
)

target_sources(Integration_Obj PRIVATE
    include/TimeIntegrator.h
    include/ExplicitEuler.h
)

target_include_directories(Integration_Obj PUBLIC include)

target_include_directories(Integration_Obj PRIVATE
    ${PROJECT_SOURCE_DIR}/PDCommon/Core/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Field/include
    ${PROJECT_SOURCE_DIR}/PDCommon/BC/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Model/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Kernel/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Utils/include
)

target_link_libraries(Integration_Obj PRIVATE PDCommon)
