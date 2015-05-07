cmake_minimum_required (VERSION 2.8.11)
project (doom)

OPTION(ID_ALLOW_TOOLS "Compile Tools" ON)

MACRO(add_msvc_precompiled_header PrecompiledHeader SourcesVar)
  IF(MSVC)
    GET_FILENAME_COMPONENT(PrecompiledBasename ${PrecompiledHeader} NAME_WE)

    SET(PrecompiledSource ${CMAKE_CURRENT_BINARY_DIR}/precompiled.cpp)    
    SET(PrecompiledBinary ${CMAKE_CURRENT_BINARY_DIR}/precompiled.pch)      

    FILE(WRITE ${PrecompiledSource} "//generated cpp for precompiled header, DO NOT EDIT!\n#include \"${PrecompiledHeader}\"" )

    SET(Sources ${${SourcesVar}})

    set_property(
      SOURCE ${PrecompiledSource}
      APPEND_STRING 
      PROPERTY COMPILE_FLAGS " /Yc\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\" "
    )

    set_property(
      SOURCE ${Sources}
      APPEND_STRING 
      PROPERTY COMPILE_FLAGS " /Yu\"${PrecompiledHeader}\" /Fp\"${PrecompiledBinary}\" "
    )    

    # Add precompiled header to SourcesVar
    LIST(APPEND ${SourcesVar} ${PrecompiledSource})
  ENDIF(MSVC)
ENDMACRO()

MACRO(add_force_include Header SourcesVar)
  SET(Sources ${${SourcesVar}})
  IF(MSVC)
    set_property(
      SOURCE ${Sources}
      APPEND_STRING 
      PROPERTY COMPILE_FLAGS " /FI\"${Header}\" "
    )
  ELSE()
    set_property(
      SOURCE ${Sources}
      APPEND_STRING 
      PROPERTY COMPILE_FLAGS " -include \"${Header}\" "
    )    
  ENDIF()
ENDMACRO()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

IF(ID_ALLOW_TOOLS)
  add_definitions(-DID_ALLOW_TOOLS)
ENDIF()

IF(MSVC)
  SET(CMAKE_CXX_FLAGS         "/DWIN32 /D_WIN32 /D_WINDOWS /EHsc /GR /fp:fast /GS- /MP /W3 /D_MBCS /D_USE_32BIT_TIME_T /D_CRT_SECURE_NO_DEPRECATE /D_CRT_NONSTDC_NO_DEPRECATE")    
  SET(CMAKE_CXX_FLAGS_DEBUG   "/D_DEBUG /MTd /Od /Zi")
  SET(CMAKE_CXX_FLAGS_RELEASE "/MT /O2 /GL")  

  SET_PROPERTY(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER  "")
  SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)
ENDIF()  

SET(CMAKE_C_FLAGS         ${CMAKE_CXX_FLAGS})    
SET(CMAKE_C_FLAGS_DEBUG   ${CMAKE_CXX_FLAGS_DEBUG})
SET(CMAKE_C_FLAGS_RELEASE ${CMAKE_CXX_FLAGS_RELEASE})    

add_subdirectory (neo/idlib)
add_subdirectory (neo/curl)
add_subdirectory (neo/TypeInfo)
add_subdirectory (neo/game)
add_subdirectory (neo/d3xp)
add_subdirectory (neo/maya2md5)
add_subdirectory (neo)