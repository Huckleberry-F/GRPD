# ==============================================================================
# PDCommon/Kernel/CMakeLists.txt
# ==============================================================================

add_library(Kernel_Obj OBJECT 
    src/KernelRegistry.cpp
    src/BB_Base.cpp
    src/NOSB_Base.cpp
    src/NOSB_T.cpp
    src/ThermalStabilizers.cpp
    src/StabilizerRegistry.cpp
)

target_sources(Kernel_Obj PRIVATE
    include/PDKernel.h
    include/BB_Base.h
    include/NOSB_T.h
    include/ThermalStabilizers.h
    include/Stabilizer.h
    include/StabilizerRegistry.h
)

target_include_directories(Kernel_Obj PUBLIC include)

target_link_libraries(Kernel_Obj PRIVATE yaml-cpp OpenMP::OpenMP_CXX Utils_Obj Field_Obj Material_Obj Neighbor_Obj Core_Obj Model_Obj)
target_link_libraries(Kernel_Obj PUBLIC Eigen3::Eigen)
