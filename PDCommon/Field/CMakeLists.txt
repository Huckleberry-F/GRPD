# ==============================================================================
# PDCommon/Field/CMakeLists.txt
# ==============================================================================

# 1. 定义对象库 (OBJECT library)
add_library(Field_Obj OBJECT
    src/Field.cpp
    src/FieldManager.cpp
    src/FieldRegistry.cpp
    src/PhysicsFields.cpp
    src/PhysicsFieldRegistry.cpp
    src/ThermalFields.cpp
    src/MechanicalFields.cpp
)

# 2. 添加头文件到构建目标
target_sources(Field_Obj PRIVATE
    include/Field.h
    include/TypedField.h
    include/FieldManager.h
    include/FieldRegistry.h
    include/PhysicsFields.h
    include/PhysicsFieldRegistry.h
    include/ThermalFields.h
    include/MechanicalFields.h
)

# 3. 设置头文件 include 路径
target_include_directories(Field_Obj PUBLIC include)

# 4. 链接依赖：Field 模块依赖 Utils (Logger) 和 OpenMP
target_link_libraries(Field_Obj PUBLIC Utils_Obj OpenMP::OpenMP_CXX)
