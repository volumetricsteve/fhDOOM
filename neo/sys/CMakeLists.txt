
SET(SOURCES
  sys_local.cpp
  sys_local.h
  sys_public.h   
  )

IF(WIN32)
  SET(SOURCES ${SOURCES}
    win32/eax.h      
    #win32/rc/AFEditor.rc
    win32/rc/AFEditor_resource.h
    #win32/rc/Common.rc
    win32/rc/Common_resource.h
    win32/rc/CreateResourceIDs.cpp
    win32/rc/CreateResourceIDs.h
    #win32/rc/Debugger.rc
    win32/rc/Debugger_resource.h
    #win32/rc/DeclEditor.rc
    win32/rc/DeclEditor_resource.h
    win32/rc/doom.rc
    win32/rc/doom_resource.h
    #win32/rc/GuiEd.rc
    win32/rc/GuiEd_resource.h
    #win32/rc/MaterialEditor.rc
    win32/rc/MaterialEditor_Resource.h
    #win32/rc/ParticleEditor.rc
    win32/rc/ParticleEditor_resource.h
    #win32/rc/PDAEditor.rc
    win32/rc/PDAEditor_resource.h
    #win32/rc/PropTree.rc
    win32/rc/PropTree_resource.h
    #win32/rc/Radiant.rc
    win32/rc/Radiant_resource.h
    #win32/rc/ScriptEditor.rc
    win32/rc/ScriptEditor_resource.h
    #win32/rc/SoundEditor.rc
    win32/rc/SoundEditor_resource.h
    win32/win_cpu.cpp
    #win32/win_gamma.cpp
    win32/win_glimp.cpp
    win32/win_input.cpp
    win32/win_local.h
    win32/win_main.cpp
    win32/win_net.cpp    
    win32/win_shared.cpp
    win32/win_snd.cpp
    win32/win_syscon.cpp
    win32/win_taskkeyhook.cpp
    win32/win_wndproc.cpp
  )
ENDIF()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
add_force_include(precompiled.h SOURCES)
add_msvc_precompiled_header(precompiled.h SOURCES)

set(TARGET_NAME "syslib")
add_definitions(-D__DOOM__)
add_library(${TARGET_NAME} ${SOURCES})

if(MSVC)
  source_group(Sys .*)  
  source_group(Sys\\win32 win32/.*)    
  set_property(TARGET ${TARGET_NAME} PROPERTY FOLDER libs)
endif()