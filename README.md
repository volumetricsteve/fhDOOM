# fhDOOM

## About

fhDOOM is one of my little side projects. I am working on this because its fun and a great learning experience. Its interesting to explore the code and to test out how such an old game can be improved by new features and more modern techniques. I have no plans to develop a real game or something like that with it. Using Unity, Cry Engine or Unreal Engine might be a better choice for that anyways.

### Goals
  * Keep it easy to build
    * simple build system
    * compiles with modern compilers
    * minimal dependencies
  * Keep the original game working, all of it!
    * The game itself
    * The expansion pack Resurrection of Evil
    * The Tools
  * Provide a good 'out of the box' experience: easy installation and don't make the user fiddle with cvars or copying shader files around
  * Stay true to the original game. Enhancements are fine, as long as they don't look out of place or destroy the mood of the game.
  * Support for Windows and Linux

### Similar Projects
id software released a modernized version of DOOM3 and its engine as "DOOM 3 - BFG edition". There are also other forks of the DOOM3 engine out there, that you might want to check out:
 * iodoom3 (discontinued?)
 * dwhbem
 * rbdoom (based on DOOM-3-BFG)
 * Dark Mod (total conversion, not sure if it can still run the original DOOM3 game)

## Changes
 * CMake build system
 * Compiles fine with Visual Studio 2013 (you need to have the MFC Multibyte Addon installed)
 * Replaced deprecated DirectX SDK by Windows 8.1 SDK 
 * Use GLEW for OpenGL extension handling (replaced all qgl\* calls by normal gl\* calls)
 * Contains all game assets from latest patch 1.31 (freely available, no need to install) 
 * OpenGL 4.3 core profile  
  * Fixed function and ARB2 assembly shaders are completely gone
  * Re-wrote everything with GLSL  
  * Game and Tools!
 * Soft shadows via shadow mapping
  * Alpha-tested surfaces can cast shadows 
  * Configurable globally as a default (useful for original maps) and for each light individually
    * Softness
    * Brightness
  * Shadow mapping and Stencil Shadows can be freely mixed
 * Soft particles
 * Parallax occlusion mapping (expects height data in the alpha channel of the specular map)
 * Added support for Qt based tools
   * optional (can be excluded from the build, so you are not forced to install Qt to build fhDOOM)
   * Implemented some basic helpers
    * RenderWidget, so the engine can render easily to the GUI
    * several input widgets for colors, numbers and vectors
    * utility functions to convert between Qt and id types (Strings, Colors, etc.)
   * re-implemented the light editor as an example (+some improvements)
    * useable ingame and from editor/radiant
    * cleaned up layout 
    * removed unused stuff
    * added advanced shadow options (related to shadow mapping)
    * show additional material properties (eg the file where it is defined)
 * Several smaller changes
  * Detect base directory automatically by walking up current directory path
  * Added new r_mode's for HD/widescreen resolutions (r_mode also sets the aspect ratio)
  * Set font size of console via con_fontScale
  * Set size of console via con_size
  * Minor fixes and cleanups in editor code (e.g. fixed auto save, removed some unused options)
  * Renamed executable and game dll (the intention was to have fhDOOM installed next to the original doom binaries. Not sure if that still works)
  * Moved maya import stuff from dll to a stand-alone command line tool (maya2md5)
  * Compile most 3rd party stuff as separate libraries
  * Fixed tons of warnings

### Notes
  * The maps of the original game were not designed with shadow mapping in mind. The engine tries to find sensible shadow parameters, but those parameters are not the perfect fit in every case, so if you look closely enough you will notice a few glitches here and there
    * light bleeding
    * low-res/blocky shadows
  * Parallax Occlusion Mapping is disabled by default, because its look just weird and wrong in a lot of places (I haven't put much work into it yet. Its basically just a quick test i did because its an often cited sikk mod feature)
  * Shadow maps are not yet implemented for parallel/directional lights (fall back to stencil shadows)
  * Shaders from RoE expansion pack not rewritten yet
  * There was a time, where you could switch from core profile to compatibility profile (r_glCoreProfile). In compatibility profile, you were still able to use ARB2 shaders for special effects (like heat haze). This feature stopped working for some reason, i haven't investigated the code for the exact reason yet. Not sure if i will ever do so. I am fine with core profile.
  * Only tested on nVidia hardware
  * Doom3's editor code is still a giant mess  
  * I added support for Qt based tools purely out of curiosity. I have currently no plans to port more tools to Qt.  
  * Other stuff on my ToDo list (no particular order):
    * Some ideas to improve render performance (related to shadow mapping). The engine runs pretty decent on my (fairly dated) machine, not sure if its worth the effort. Might change with more advanced rendering features.
    * Poisson sampling for shadow maps 
    * HDR rendering/Bloom
    * Tesselation/Displacement Mapping
    * Multi threading (primarily to improve loading times. Not sure entirely yet how to parallelize the rendering or game code)    
    * Hardware/GPU skinning
    * Port MFC stuff to unicode (so you don't need that Multibyte Addon, less dependencies)
    * Doom3 contains a very early and simple form of Megatexturing. Might be interesting to re-enable that for modern OpenGL and GLSL.
    * Rework available graphic options 
      * not sure if they all work properly
      * some of them make no sense on a modern PC (e.g. you don't want to disable specular maps)
    * 64bit support would simplify the build process on linux (totally irrelevant on windows, the actual game doesn't really need that much memory...)
      * Get rid of the ASM code. Rewrite with compiler intrinsics? Not sure if its worth the effort... the generic C/C++ implementation might be sufficient (needs some testing)
      * Get rid of EAX stuff
    * Look into Doom3's texture compression and modern alternatives. Does the wulfen texture pack (and others) really need to be that big? How does it effect rendering performance?
    * Render everything to an offscreen buffer
      * super sampling
      * Default framebuffer does always match your native display resolution. (fast switching between different r_modes, simplifies messy fullscreen switch)
  * i am pretty sure i forgot a lot of things, so you might discover more things that are pretty hacky or not working at all ;)
 