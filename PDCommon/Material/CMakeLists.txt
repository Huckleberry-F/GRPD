# ==============================================================================
# PDCommon/Material/CMakeLists.txt
# ==============================================================================

# 1. Define object library (produces .obj, not .lib)
# Manually list source files for CMake 4.0+ stability
add_library(Material_Obj OBJECT 
    src/Material.cpp
    src/MaterialManager.cpp
    src/MaterialRegistry.cpp
    src/FourierThermalMat.cpp
    src/LinearElasticMat.cpp
    src/J2PlasticityMat.cpp
    src/JCPlasticityMat.cpp
    src/J2VoceLemaitreMat.cpp
    src/CreepMaterialBase.cpp
    src/NortonBaileyCreepMat.cpp
    src/TimeHardeningCreepMat.cpp
    src/StrainHardeningCreepMat.cpp
)

# 2. Add header files to target
target_sources(Material_Obj PRIVATE
    include/Material.h
    include/MaterialManager.h
    include/MaterialRegistry.h
    include/ThermalMaterial.h
    include/FourierThermalMat.h
    include/MechanicalMaterial.h
    include/LinearElasticMat.h
    include/J2PlasticityMat.h
    include/JCPlasticityMat.h
    include/J2VoceLemaitreMat.h
    include/CreepMaterialBase.h
    include/NortonBaileyCreepMat.h
    include/TimeHardeningCreepMat.h
    include/StrainHardeningCreepMat.h
)

# 3. Set header include paths for this module
# PUBLIC ensures targets linking to Material_Obj can see these headers
target_include_directories(Material_Obj PUBLIC include)
target_include_directories(Material_Obj PRIVATE ${PROJECT_SOURCE_DIR}/PDCommon/Fracture/include)
target_include_directories(Material_Obj PRIVATE ${PROJECT_SOURCE_DIR}/PDCommon/Core/include)
target_include_directories(Material_Obj PRIVATE ${PROJECT_SOURCE_DIR}/PDCommon/BC/include)
target_include_directories(Material_Obj PRIVATE ${PROJECT_SOURCE_DIR}/PDCommon/Contact/include)
target_include_directories(Material_Obj PRIVATE ${PROJECT_SOURCE_DIR}/PDCommon/Model/include)
target_include_directories(Material_Obj PRIVATE ${PROJECT_SOURCE_DIR}/PDCommon/Neighbor/include)

# 4. Link dependencies
# Note: For OBJECT libraries, this is mainly to propagate include paths
# Eigen3::Eigen 需要 PUBLIC 因为 ThermalMaterial.h 公共头文件包含了 <Eigen/Dense>
target_link_libraries(Material_Obj PRIVATE yaml-cpp OpenMP::OpenMP_CXX Utils_Obj Field_Obj)
target_link_libraries(Material_Obj PUBLIC Eigen3::Eigen)