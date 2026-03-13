# ==============================================================================
# PDCommon/CMakeLists.txt
# ==============================================================================
# 1. Find OpenMP package (REQUIRED means error if not found)
find_package(OpenMP REQUIRED)

# 2. OpenMP::OpenMP_CXX target exists only if found
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found and version is ${OpenMP_CXX_VERSION}")
endif()

# 1. Enter subdirectories first so CMake defines Model_Obj, IO_Obj, etc.
add_subdirectory(Utils)
add_subdirectory(Field)
add_subdirectory(Material)
add_subdirectory(Model)
add_subdirectory(BC)
add_subdirectory(Initial)
add_subdirectory(IO)
add_subdirectory(Core)

# 2. Create the final static library and bundle all submodule object files
# $<TARGET_OBJECTS:target_name> is the standard CMake syntax for referencing object libraries
add_library(PDCommon STATIC
    $<TARGET_OBJECTS:Model_Obj>
    $<TARGET_OBJECTS:Field_Obj>
	$<TARGET_OBJECTS:Initial_Obj>
    $<TARGET_OBJECTS:IO_Obj>
    $<TARGET_OBJECTS:Material_Obj>
    $<TARGET_OBJECTS:BC_Obj>
    $<TARGET_OBJECTS:Utils_Obj>
    $<TARGET_OBJECTS:Core_Obj>
)

# 3. Propagate header file interfaces uniformly
# Although objects are bundled, we still need to expose each module's include paths externally (e.g. to Src)
target_link_libraries(PDCommon PUBLIC 
    Model_Obj 
    Field_Obj
    Utils_Obj
    Material_Obj
    BC_Obj
    Initial_Obj
    IO_Obj
    Core_Obj
    yaml-cpp
    OpenMP::OpenMP_CXX
)
