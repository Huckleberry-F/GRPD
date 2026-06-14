# ==============================================================================
# PDCommon/CMakeLists.txt
# ==============================================================================
# 1. For macOS (Apple) with AppleClang, try to find Homebrew's libomp to satisfy OpenMP dependency
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
    # Search for omp.h in standard Homebrew paths
    find_path(OpenMP_INCLUDES omp.h HINTS "/opt/homebrew/opt/libomp/include" "/usr/local/opt/libomp/include")
    # Search for libomp in standard Homebrew paths
    find_library(OpenMP_LIBRARIES omp HINTS "/opt/homebrew/opt/libomp/lib" "/usr/local/opt/libomp/lib")
    
    if(OpenMP_INCLUDES AND OpenMP_LIBRARIES)
        set(OpenMP_CXX_FLAGS "-Xpreprocessor -fopenmp")
        set(OpenMP_CXX_FOUND TRUE)
        set(OpenMP_FOUND TRUE)
        if(NOT TARGET OpenMP::OpenMP_CXX)
            add_library(OpenMP::OpenMP_CXX IMPORTED INTERFACE)
            set_target_properties(OpenMP::OpenMP_CXX PROPERTIES
                INTERFACE_COMPILE_OPTIONS "-Xpreprocessor;-fopenmp"
                INTERFACE_INCLUDE_DIRECTORIES "${OpenMP_INCLUDES}"
                INTERFACE_LINK_LIBRARIES "${OpenMP_LIBRARIES}"
            )
        endif()
        message(STATUS "macOS AppleClang: Found Homebrew OpenMP at ${OpenMP_INCLUDES}")
    endif()
endif()

# Find OpenMP package (if not already found via our custom AppleClang helper)
if(NOT OpenMP_FOUND)
    find_package(OpenMP REQUIRED)
endif()

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
add_subdirectory(Neighbor)
add_subdirectory(IO)
add_subdirectory(Core)
add_subdirectory(Kernel)
add_subdirectory(Fracture)
add_subdirectory(Contact)
add_subdirectory(PostProcessing)

# 2. Create the final static library and bundle all submodule object files
# $<TARGET_OBJECTS:target_name> is the standard CMake syntax for referencing object libraries
add_library(PDCommon STATIC
    $<TARGET_OBJECTS:Model_Obj>
    $<TARGET_OBJECTS:Field_Obj>
	$<TARGET_OBJECTS:Neighbor_Obj>
    $<TARGET_OBJECTS:IO_Obj>
    $<TARGET_OBJECTS:Material_Obj>
    $<TARGET_OBJECTS:BC_Obj>
    $<TARGET_OBJECTS:Utils_Obj>
    $<TARGET_OBJECTS:Core_Obj>
    $<TARGET_OBJECTS:Kernel_Obj>
    $<TARGET_OBJECTS:Fracture_Obj>
    $<TARGET_OBJECTS:Contact_Obj>
    $<TARGET_OBJECTS:PostProcessing_Obj>
)

# 3. Propagate header file interfaces uniformly
# Although objects are bundled, we still need to expose each module's include paths externally (e.g. to Src)
target_link_libraries(PDCommon PUBLIC 
    Model_Obj 
    Field_Obj
    Utils_Obj
    Material_Obj
    BC_Obj
    Neighbor_Obj
    IO_Obj
    Core_Obj
    Kernel_Obj
    Fracture_Obj
    Contact_Obj
    PostProcessing_Obj
    yaml-cpp
    OpenMP::OpenMP_CXX
)
