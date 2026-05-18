# ==============================================================================
# PDCommon/PostProcessing/CMakeLists.txt
# ==============================================================================

set(POSTPROCESSING_SRCS
    src/PostProcessorRegistry.cpp
    src/PostProcessorManager.cpp
    src/ReactionForceExtractor.cpp
)

add_library(PostProcessing_Obj OBJECT ${POSTPROCESSING_SRCS})

target_include_directories(PostProcessing_Obj PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Field/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Model/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Material/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Utils/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../BC/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Neighbor/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../Contact/include
    ${PROJECT_SOURCE_DIR}/Third/eigen
)

target_link_libraries(PostProcessing_Obj PUBLIC yaml-cpp OpenMP::OpenMP_CXX)
