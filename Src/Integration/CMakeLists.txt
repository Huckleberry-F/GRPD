# ==============================================================================
# Src/Integration/CMakeLists.txt
# ==============================================================================

add_library(Integration_Obj OBJECT 
    src/TimeIntegrator.cpp
    src/ExplicitEuler.cpp
    src/TimeIntegratorRegistry.cpp
    src/CentralDifference.cpp
    src/StaggeredIntegrator.cpp
    src/ADR_Integrator.cpp
    src/MatrixFreeImplicitIntegrator.cpp
)

target_sources(Integration_Obj PRIVATE
    include/TimeIntegrator.h
    include/ExplicitEuler.h
    include/TimeIntegratorRegistry.h
    include/CentralDifference.h
    include/StaggeredIntegrator.h
    include/ADR_Integrator.h
    include/MatrixFreeImplicitIntegrator.h
)

target_include_directories(Integration_Obj PUBLIC include)

target_include_directories(Integration_Obj PRIVATE
    ${PROJECT_SOURCE_DIR}/PDCommon/Core/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Field/include
    ${PROJECT_SOURCE_DIR}/PDCommon/BC/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Model/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Kernel/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Utils/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Contact/include
)

target_link_libraries(Integration_Obj PRIVATE PDCommon)
