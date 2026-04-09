# ==============================================================================
# Src - Main solver engine
# ==============================================================================

# 1. Define executable target
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} DIR_SRCS)
add_executable(${PROJECT_NAME} ${DIR_SRCS})

# Add include directories for PDCommon components
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/PDCommon/Utils/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Material/include
    ${PROJECT_SOURCE_DIR}/PDCommon/BC/include
    ${PROJECT_SOURCE_DIR}/PDCommon/Core/include
    ${PROJECT_SOURCE_DIR}/Src
    ${PROJECT_SOURCE_DIR}/Src/Engine/include
)

# 2. Link PDCommon base library and all components to force static registration logic inclusion
target_link_libraries(${PROJECT_NAME} PRIVATE 
    PDCommon 
    Engine_Obj 
    PDSolve_Obj
    Material_Obj 
    BC_Obj 
    IO_Obj 
    Model_Obj 
    Neighbor_Obj
    Core_Obj
    Field_Obj
    Kernel_Obj
    Integration_Obj
    Fracture_Obj
)


# 3. Add Engine component
add_subdirectory(Engine)
add_subdirectory(Integration)