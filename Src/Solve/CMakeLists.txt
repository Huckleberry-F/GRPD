# 1. Expose current directory (contains AbstractSolver.h and SolverManager.h)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 2. Include subdirectory headers (so SolverManager can see concrete implementations)
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/Mechanical
    ${CMAKE_CURRENT_SOURCE_DIR}/Thermal
    ${CMAKE_CURRENT_SOURCE_DIR}/Coupled
)

# 3. Recursively collect all physics solver source files
file(GLOB_RECURSE SOLVER_SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

# 4. Inject into main executable target
target_sources(${PROJECT_NAME} PRIVATE ${SOLVER_SOURCES})