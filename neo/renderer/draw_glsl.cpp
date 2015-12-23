#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"
#include "ImmediateMode.h"

void RB_GLSL_GetShadowParams(float* minBias, float* maxBias, float* fuzzyness, int* samples);

idCVar r_pomEnabled("r_pomEnabled", "0", CVAR_ARCHIVE | CVAR_RENDERER | CVAR_BOOL, "POM enabled or disabled");
idCVar r_pomMaxHeight("r_pomMaxHeight", "0.045", CVAR_ARCHIVE | CVAR_RENDERER | CVAR_FLOAT, "maximum height for POM");
idCVar r_shading("r_shading", "0", CVAR_ARCHIVE | CVAR_RENDERER | CVAR_INTEGER, "0 = Doom3 (Blinn-Phong?), 1 = Phong");
idCVar r_specularExp("r_specularExp", "10", CVAR_ARCHIVE | CVAR_RENDERER | CVAR_FLOAT, "exponent used for specularity");
idCVar r_specularScale("r_specularScale", "1", CVAR_ARCHIVE | CVAR_RENDERER | CVAR_FLOAT, "scale specularity globally for all surfaces");

#define MAX_GLPROGS 128
static glslProgramDef_t glslPrograms[MAX_GLPROGS] = { 0 };

const glslProgramDef_t* shadowProgram = nullptr;
const glslProgramDef_t* interactionProgram = nullptr;
const glslProgramDef_t* depthProgram = nullptr;
const glslProgramDef_t* defaultProgram = nullptr;
const glslProgramDef_t* depthblendProgram = nullptr;
const glslProgramDef_t* skyboxProgram = nullptr;
const glslProgramDef_t* bumpyEnvProgram = nullptr;
const glslProgramDef_t* fogLightProgram = nullptr;
const glslProgramDef_t* blendLightProgram = nullptr;
const glslProgramDef_t* vertexColorProgram = nullptr;
const glslProgramDef_t* flatColorProgram = nullptr;
const glslProgramDef_t* intensityProgram = nullptr;

/*
====================
GL_SelectTextureNoClient
====================
*/
static void GL_SelectTextureNoClient(int unit) {
  backEnd.glState.currenttmu = unit;
  glActiveTextureARB(GL_TEXTURE0_ARB + unit);
  RB_LogComment("glActiveTextureARB( %i )\n", unit);
}

/*
====================
attributeOffset

Calculate attribute offset by a (global) offset and (local) per-attribute offset
====================
*/
template<typename T>
static const void* attributeOffset(T offset, const void* attributeOffset)
{
  return reinterpret_cast<const void*>((std::ptrdiff_t)offset + (std::ptrdiff_t)attributeOffset);
}

/*
=====================
RB_GLSL_BlendLight

=====================
*/
static void RB_GLSL_BlendLight(const drawSurf_t *surf) {
  const srfTriangles_t *tri;

  tri = surf->geo;

  glUniformMatrix4fv(glslProgramDef_t::uniform_modelViewMatrix, 1, false, GL_ModelViewMatrix.Top());
  glUniformMatrix4fv(glslProgramDef_t::uniform_projectionMatrix, 1, false, GL_ProjectionMatrix.Top());

  if (backEnd.currentSpace != surf->space) {
    idPlane	lightProject[4];
    int		i;

    for (i = 0; i < 4; i++) {
      R_GlobalPlaneToLocal(surf->space->modelMatrix, backEnd.vLight->lightProject[i], lightProject[i]);
    }

    glUniform4fv(glslProgramDef_t::uniform_bumpMatrixS, 1, lightProject[0].ToFloatPtr());
    glUniform4fv(glslProgramDef_t::uniform_bumpMatrixT, 1, lightProject[1].ToFloatPtr());
    glUniform4fv(glslProgramDef_t::uniform_specularMatrixS, 1, lightProject[2].ToFloatPtr());
    glUniform4fv(glslProgramDef_t::uniform_diffuseMatrixS, 1, lightProject[3].ToFloatPtr());
/*
    GL_SelectTexture(0);
    glTexGenfv(GL_S, GL_OBJECT_PLANE, lightProject[0].ToFloatPtr());
    glTexGenfv(GL_T, GL_OBJECT_PLANE, lightProject[1].ToFloatPtr());
    glTexGenfv(GL_Q, GL_OBJECT_PLANE, lightProject[2].ToFloatPtr());

    GL_SelectTexture(1);
    glTexGenfv(GL_S, GL_OBJECT_PLANE, lightProject[3].ToFloatPtr());
*/
  }

  // this gets used for both blend lights and shadow draws
  if (tri->ambientCache) {
    //idDrawVert	*ac = (idDrawVert *)vertexCache.Position(tri->ambientCache);
    //glVertexPointer(3, GL_FLOAT, sizeof(idDrawVert), ac->xyz.ToFloatPtr());
    //     
    int offset = vertexCache.Bind(tri->ambientCache);
    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_position, 3, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::xyzOffset));
  }
  else if (tri->shadowCache) {
    //shadowCache_t	*sc = (shadowCache_t *)vertexCache.Position(tri->shadowCache);
    //glVertexPointer(3, GL_FLOAT, sizeof(shadowCache_t), sc->xyz.ToFloatPtr());
    int offset = vertexCache.Bind(tri->shadowCache);
    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_position_shadow);
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_position_shadow, 3, GL_FLOAT, false, sizeof(shadowCache_t), attributeOffset(offset, 0));
  }

  RB_DrawElementsWithCounters(tri);

  if (tri->ambientCache) {   
    glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
  }
  else if (tri->shadowCache) {
    glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_position_shadow);
  }
}


/*
=====================
RB_GLSL_BlendLight

Dual texture together the falloff and projection texture with a blend
mode to the framebuffer, instead of interacting with the surface texture
=====================
*/
void RB_GLSL_BlendLight(const drawSurf_t *drawSurfs, const drawSurf_t *drawSurfs2) {
  const idMaterial	*lightShader;
  const shaderStage_t	*stage;
  int					i;
  const float	*regs;

  if (!drawSurfs) {
    return;
  }
  if (r_skipBlendLights.GetBool()) {
    return;
  }
  RB_LogComment("---------- RB_GLSL_BlendLight ----------\n");

  GL_UseProgram(blendLightProgram);

  lightShader = backEnd.vLight->lightShader;
  regs = backEnd.vLight->shaderRegisters;

  // texture 1 will get the falloff texture
  GL_SelectTexture(1);  
  backEnd.vLight->falloffImage->Bind();

  for (i = 0; i < lightShader->GetNumStages(); i++) {
    stage = lightShader->GetStage(i);

    if (!regs[stage->conditionRegister]) {
      continue;
    }

    GL_State(GLS_DEPTHMASK | stage->drawStateBits | GLS_DEPTHFUNC_EQUAL);

    // texture 0 will get the projected texture
    GL_SelectTexture(0);
    stage->texture.image->Bind();

    if (stage->texture.hasMatrix) {
      RB_LoadShaderTextureMatrix(regs, &stage->texture);
    }

    // get the modulate values from the light, including alpha, unlike normal lights
    backEnd.lightColor[0] = regs[stage->color.registers[0]];
    backEnd.lightColor[1] = regs[stage->color.registers[1]];
    backEnd.lightColor[2] = regs[stage->color.registers[2]];
    backEnd.lightColor[3] = regs[stage->color.registers[3]];
    glUniform4fv(glslProgramDef_t::uniform_diffuse_color, 1, backEnd.lightColor);

    RB_RenderDrawSurfChainWithFunction(drawSurfs, RB_GLSL_BlendLight);
    RB_RenderDrawSurfChainWithFunction(drawSurfs2, RB_GLSL_BlendLight);

    if (stage->texture.hasMatrix) {
      GL_SelectTexture(0);
      GL_TextureMatrix.LoadIdentity();
    }
  }
}


/*
==================
R_WobbleskyTexGen
==================
*/
static void R_CreateWobbleskyTexMatrix(const drawSurf_t *surf, float time, float matrix[16]) {
  
  const int *parms = surf->material->GetTexGenRegisters();

  float	wobbleDegrees = surf->shaderRegisters[parms[0]];
  float	wobbleSpeed = surf->shaderRegisters[parms[1]];
  float	rotateSpeed = surf->shaderRegisters[parms[2]];

  wobbleDegrees = wobbleDegrees * idMath::PI / 180;
  wobbleSpeed = wobbleSpeed * 2 * idMath::PI / 60;
  rotateSpeed = rotateSpeed * 2 * idMath::PI / 60;

  // very ad-hoc "wobble" transform  
  float	a = time * wobbleSpeed;
  float	s = sin(a) * sin(wobbleDegrees);
  float	c = cos(a) * sin(wobbleDegrees);
  float	z = cos(wobbleDegrees);

  idVec3	axis[3];

  axis[2][0] = c;
  axis[2][1] = s;
  axis[2][2] = z;

  axis[1][0] = -sin(a * 2) * sin(wobbleDegrees);
  axis[1][2] = -s * sin(wobbleDegrees);
  axis[1][1] = sqrt(1.0f - (axis[1][0] * axis[1][0] + axis[1][2] * axis[1][2]));

  // make the second vector exactly perpendicular to the first
  axis[1] -= (axis[2] * axis[1]) * axis[2];
  axis[1].Normalize();

  // construct the third with a cross
  axis[0].Cross(axis[1], axis[2]);

  // add the rotate
  s = sin(rotateSpeed * time);
  c = cos(rotateSpeed * time);
  
  matrix[0] = axis[0][0] * c + axis[1][0] * s;
  matrix[4] = axis[0][1] * c + axis[1][1] * s;
  matrix[8] = axis[0][2] * c + axis[1][2] * s;

  matrix[1] = axis[1][0] * c - axis[0][0] * s;
  matrix[5] = axis[1][1] * c - axis[0][1] * s;
  matrix[9] = axis[1][2] * c - axis[0][2] * s;

  matrix[2] = axis[2][0];
  matrix[6] = axis[2][1];
  matrix[10] = axis[2][2];

  matrix[3] = matrix[7] = matrix[11] = 0.0f;
  matrix[12] = matrix[13] = matrix[14] = 0.0f;
}

/*
===============
RB_RenderTriangleSurface

Sets texcoord and vertex pointers
===============
*/
static void RB_GLSL_RenderTriangleSurface(const srfTriangles_t *tri) {
  if (!tri->ambientCache) {
    RB_DrawElementsImmediate(tri);
    return;
  }

  auto offset = vertexCache.Bind(tri->ambientCache);

  glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
  glVertexAttribPointer(glslProgramDef_t::vertex_attrib_position, 3, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, 0));
  RB_DrawElementsWithCounters(tri);
  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
}

static idPlane	fogPlanes[4];

/*
=====================
RB_T_BasicFog

=====================
*/
static void RB_GLSL_BasicFog(const drawSurf_t *surf) {
  glUniformMatrix4fv(glslProgramDef_t::uniform_modelViewMatrix, 1, false, GL_ModelViewMatrix.Top());
  glUniformMatrix4fv(glslProgramDef_t::uniform_projectionMatrix, 1, false, GL_ProjectionMatrix.Top());

  if (backEnd.currentSpace != surf->space) {
    idPlane	local;

    GL_SelectTexture(0);

    R_GlobalPlaneToLocal(surf->space->modelMatrix, fogPlanes[0], local);
    local[3] += 0.5;
    glUniform4fv(glslProgramDef_t::uniform_bumpMatrixS, 1, local.ToFloatPtr());
    //glTexGenfv(GL_S, GL_OBJECT_PLANE, local.ToFloatPtr());

    //		R_GlobalPlaneToLocal( surf->space->modelMatrix, fogPlanes[1], local );
    //		local[3] += 0.5;
    local[0] = local[1] = local[2] = 0; local[3] = 0.5;
    glUniform4fv(glslProgramDef_t::uniform_bumpMatrixT, 1, local.ToFloatPtr());
    //glTexGenfv(GL_T, GL_OBJECT_PLANE, local.ToFloatPtr());

    GL_SelectTexture(1);

    // GL_S is constant per viewer
    R_GlobalPlaneToLocal(surf->space->modelMatrix, fogPlanes[2], local);
    local[3] += FOG_ENTER;
    glUniform4fv(glslProgramDef_t::uniform_diffuseMatrixT, 1, local.ToFloatPtr());
    //glTexGenfv(GL_T, GL_OBJECT_PLANE, local.ToFloatPtr());

    R_GlobalPlaneToLocal(surf->space->modelMatrix, fogPlanes[3], local);
    glUniform4fv(glslProgramDef_t::uniform_diffuseMatrixS, 1, local.ToFloatPtr());
    //glTexGenfv(GL_S, GL_OBJECT_PLANE, local.ToFloatPtr());
  }

  RB_GLSL_RenderTriangleSurface(surf->geo);
}

/*
==================
R_GLSL_Init

Load default shaders.
==================
*/
void	R_GLSL_Init( void )
{
  fogLightProgram = R_FindGlslProgram("fogLight.vp", "fogLight.fp");  
  blendLightProgram = R_FindGlslProgram("blendLight.vp", "blendLight.fp");  
  shadowProgram = R_FindGlslProgram("shadow.vp", "shadow.fp");
  depthProgram = R_FindGlslProgram("depth.vp", "depth.fp");  
  defaultProgram = R_FindGlslProgram("default.vp", "default.fp");
  depthblendProgram = R_FindGlslProgram("depthblend.vp", "depthblend.fp");
  skyboxProgram = R_FindGlslProgram("skybox.vp", "skybox.fp");
  bumpyEnvProgram = R_FindGlslProgram("bumpyenv.vp", "bumpyenv.fp");  
  interactionProgram = R_FindGlslProgram("interaction.vp", "interaction.fp");
  vertexColorProgram = R_FindGlslProgram("vertexcolor.vp", "vertexcolor.fp");
  flatColorProgram = R_FindGlslProgram("flatcolor.vp", "flatcolor.fp");
  intensityProgram = R_FindGlslProgram("intensity.vp", "intensity.fp");

  fhImmediateMode::Init();
}


/*
==================
RB_GLSL_FogPass
==================
*/
void RB_GLSL_FogPass(const drawSurf_t *drawSurfs, const drawSurf_t *drawSurfs2) {
  assert(fogLightProgram);

  RB_LogComment("---------- RB_GLSL_FogPass ----------\n");  

  GL_UseProgram(fogLightProgram);

  // create a surface for the light frustom triangles, which are oriented drawn side out
  const srfTriangles_t* frustumTris = backEnd.vLight->frustumTris;

  // if we ran out of vertex cache memory, skip it
  if (!frustumTris->ambientCache) {
    return;
  }    
  
  drawSurf_t ds;
  memset(&ds, 0, sizeof(ds));
  ds.space = &backEnd.viewDef->worldSpace;
  ds.geo = frustumTris;
  ds.scissorRect = backEnd.viewDef->scissor;

  // find the current color and density of the fog
  const idMaterial *lightShader = backEnd.vLight->lightShader;
  const float	     *regs        = backEnd.vLight->shaderRegisters;
  // assume fog shaders have only a single stage
  const shaderStage_t	*stage = lightShader->GetStage(0);

  backEnd.lightColor[0] = regs[stage->color.registers[0]];
  backEnd.lightColor[1] = regs[stage->color.registers[1]];
  backEnd.lightColor[2] = regs[stage->color.registers[2]];
  backEnd.lightColor[3] = regs[stage->color.registers[3]];
  
  glUniform4fv(glslProgramDef_t::uniform_diffuse_color, 1, backEnd.lightColor);

  // calculate the falloff planes
  float	a;

  // if they left the default value on, set a fog distance of 500
  if (backEnd.lightColor[3] <= 1.0) {
    a = -0.5f / DEFAULT_FOG_DISTANCE;
  }
  else {
    // otherwise, distance = alpha color
    a = -0.5f / backEnd.lightColor[3];
  }  

  // texture 0 is the falloff image
  GL_SelectTexture(0);
  globalImages->fogImage->Bind();

  fogPlanes[0][0] = a * backEnd.viewDef->worldSpace.modelViewMatrix[2];
  fogPlanes[0][1] = a * backEnd.viewDef->worldSpace.modelViewMatrix[6];
  fogPlanes[0][2] = a * backEnd.viewDef->worldSpace.modelViewMatrix[10];
  fogPlanes[0][3] = a * backEnd.viewDef->worldSpace.modelViewMatrix[14];

  fogPlanes[1][0] = a * backEnd.viewDef->worldSpace.modelViewMatrix[0];
  fogPlanes[1][1] = a * backEnd.viewDef->worldSpace.modelViewMatrix[4];
  fogPlanes[1][2] = a * backEnd.viewDef->worldSpace.modelViewMatrix[8];
  fogPlanes[1][3] = a * backEnd.viewDef->worldSpace.modelViewMatrix[12];

  // texture 1 is the entering plane fade correction
  GL_SelectTexture(1);
  globalImages->fogEnterImage->Bind();

  // T will get a texgen for the fade plane, which is always the "top" plane on unrotated lights
  fogPlanes[2][0] = 0.001f * backEnd.vLight->fogPlane[0];
  fogPlanes[2][1] = 0.001f * backEnd.vLight->fogPlane[1];
  fogPlanes[2][2] = 0.001f * backEnd.vLight->fogPlane[2];
  fogPlanes[2][3] = 0.001f * backEnd.vLight->fogPlane[3];

  // S is based on the view origin
  float s = backEnd.viewDef->renderView.vieworg * fogPlanes[2].Normal() + fogPlanes[2][3];

  fogPlanes[3][0] = 0;
  fogPlanes[3][1] = 0;
  fogPlanes[3][2] = 0;
  fogPlanes[3][3] = FOG_ENTER + s;

  // draw it
  backEnd.glState.forceGlState = true;
  GL_State(GLS_DEPTHMASK | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL);
  RB_RenderDrawSurfChainWithFunction(drawSurfs, RB_GLSL_BasicFog);
  RB_RenderDrawSurfChainWithFunction(drawSurfs2, RB_GLSL_BasicFog);

  // the light frustum bounding planes aren't in the depth buffer, so use depthfunc_less instead
  // of depthfunc_equal
  GL_State(GLS_DEPTHMASK | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_LESS);
  GL_Cull(CT_BACK_SIDED);
  RB_RenderDrawSurfChainWithFunction(&ds, RB_GLSL_BasicFog);
  GL_Cull(CT_FRONT_SIDED);

  GL_SelectTexture(1);
  globalImages->BindNull();

  GL_SelectTexture(0);
  
  GL_UseProgram(nullptr);
}




/*
==============================================================================

BACK END RENDERING OF STENCIL SHADOWS

==============================================================================
*/


/*
=====================
RB_GLSL_Shadow

the shadow volumes face INSIDE
=====================
*/

static void RB_GLSL_Shadow(const drawSurf_t *surf) {
  const srfTriangles_t	*tri;

  // set the light position if we are using a vertex program to project the rear surfaces
  if (surf->space != backEnd.currentSpace) {
    idVec4 localLight;

    R_GlobalPointToLocal(surf->space->modelMatrix, backEnd.vLight->globalLightOrigin, localLight.ToVec3());
    localLight.w = 0.0f;

    assert(shadowProgram);
    glUniform4fv(glslProgramDef_t::uniform_localLightOrigin, 1, localLight.ToFloatPtr());
    glUniformMatrix4fv(glslProgramDef_t::uniform_projectionMatrix, 1, false, GL_ProjectionMatrix.Top());
    glUniformMatrix4fv(glslProgramDef_t::uniform_modelViewMatrix, 1, false, GL_ModelViewMatrix.Top());
  }

  tri = surf->geo;

  if (!tri->shadowCache) {
    return;
  }

  const auto offset = vertexCache.Bind(tri->shadowCache);
  glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_position_shadow);
  glVertexAttribPointer(glslProgramDef_t::vertex_attrib_position_shadow, 4, GL_FLOAT, false, sizeof(shadowCache_t), attributeOffset(offset, 0));

  // we always draw the sil planes, but we may not need to draw the front or rear caps
  int	numIndexes;
  bool external = false;

  if (!r_useExternalShadows.GetInteger()) {
    numIndexes = tri->numIndexes;
  }
  else if (r_useExternalShadows.GetInteger() == 2) { // force to no caps for testing
    numIndexes = tri->numShadowIndexesNoCaps;
  }
  else if (!(surf->dsFlags & DSF_VIEW_INSIDE_SHADOW)) {
    // if we aren't inside the shadow projection, no caps are ever needed needed
    numIndexes = tri->numShadowIndexesNoCaps;
    external = true;
  }
  else if (!backEnd.vLight->viewInsideLight && !(surf->geo->shadowCapPlaneBits & SHADOW_CAP_INFINITE)) {
    // if we are inside the shadow projection, but outside the light, and drawing
    // a non-infinite shadow, we can skip some caps
    if (backEnd.vLight->viewSeesShadowPlaneBits & surf->geo->shadowCapPlaneBits) {
      // we can see through a rear cap, so we need to draw it, but we can skip the
      // caps on the actual surface
      numIndexes = tri->numShadowIndexesNoFrontCaps;
    }
    else {
      // we don't need to draw any caps
      numIndexes = tri->numShadowIndexesNoCaps;
    }
    external = true;
  }
  else {
    // must draw everything
    numIndexes = tri->numIndexes;
  }

  // set depth bounds
  if (glConfig.depthBoundsTestAvailable && r_useDepthBoundsTest.GetBool()) {
    glDepthBoundsEXT(surf->scissorRect.zmin, surf->scissorRect.zmax);
  }

  // debug visualization
  if (r_showShadows.GetInteger()) {
    if (r_showShadows.GetInteger() == 3) {
      if (external) {
        glColor3f(0.1 / backEnd.overBright, 1 / backEnd.overBright, 0.1 / backEnd.overBright);
      }
      else {
        // these are the surfaces that require the reverse
        glColor3f(1 / backEnd.overBright, 0.1 / backEnd.overBright, 0.1 / backEnd.overBright);
      }
    }
    else {
      // draw different color for turboshadows
      if (surf->geo->shadowCapPlaneBits & SHADOW_CAP_INFINITE) {
        if (numIndexes == tri->numIndexes) {
          glColor3f(1 / backEnd.overBright, 0.1 / backEnd.overBright, 0.1 / backEnd.overBright);
        }
        else {
          glColor3f(1 / backEnd.overBright, 0.4 / backEnd.overBright, 0.1 / backEnd.overBright);
        }
      }
      else {
        if (numIndexes == tri->numIndexes) {
          glColor3f(0.1 / backEnd.overBright, 1 / backEnd.overBright, 0.1 / backEnd.overBright);
        }
        else if (numIndexes == tri->numShadowIndexesNoFrontCaps) {
          glColor3f(0.1 / backEnd.overBright, 1 / backEnd.overBright, 0.6 / backEnd.overBright);
        }
        else {
          glColor3f(0.6 / backEnd.overBright, 1 / backEnd.overBright, 0.1 / backEnd.overBright);
        }
      }
    }

    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glDisable(GL_STENCIL_TEST);
    GL_Cull(CT_TWO_SIDED);
    RB_DrawShadowElementsWithCounters(tri, numIndexes);
    GL_Cull(CT_FRONT_SIDED);
    glEnable(GL_STENCIL_TEST);
    glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_position_shadow);

    return;
  }

  // patent-free work around
  if (!external) {
    // "preload" the stencil buffer with the number of volumes
    // that get clipped by the near or far clip plane
    glStencilOp(GL_KEEP, tr.stencilDecr, tr.stencilDecr);
    GL_Cull(CT_FRONT_SIDED);
    RB_DrawShadowElementsWithCounters(tri, numIndexes);
    glStencilOp(GL_KEEP, tr.stencilIncr, tr.stencilIncr);
    GL_Cull(CT_BACK_SIDED);
    RB_DrawShadowElementsWithCounters(tri, numIndexes);
  }

  // traditional depth-pass stencil shadows
  glStencilOp(GL_KEEP, GL_KEEP, tr.stencilIncr);
  GL_Cull(CT_FRONT_SIDED);
  RB_DrawShadowElementsWithCounters(tri, numIndexes);

  glStencilOp(GL_KEEP, GL_KEEP, tr.stencilDecr);
  GL_Cull(CT_BACK_SIDED);
  RB_DrawShadowElementsWithCounters(tri, numIndexes);

  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_position_shadow);
}


/*
=====================
RB_GLSL_StencilShadowPass

Stencil test should already be enabled, and the stencil buffer should have
been set to 128 on any surfaces that might receive shadows
=====================
*/



void RB_GLSL_StencilShadowPass(const drawSurf_t *drawSurfs) {
  assert(shadowProgram);

  if (!r_shadows.GetBool()) {
    return;
  }

  if (!drawSurfs) {
    return;
  }

  //glDisable(GL_VERTEX_PROGRAM_ARB);
  //glDisable(GL_FRAGMENT_PROGRAM_ARB);
  GL_UseProgram(shadowProgram);

  RB_LogComment("---------- RB_StencilShadowPass ----------\n");

  globalImages->BindNull();

  // for visualizing the shadows
  if (r_showShadows.GetInteger()) {
    if (r_showShadows.GetInteger() == 2) {
      // draw filled in
      GL_State(GLS_DEPTHMASK | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_LESS);
    }
    else {
      // draw as lines, filling the depth buffer
      GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO | GLS_POLYMODE_LINE | GLS_DEPTHFUNC_ALWAYS);
    }
  }
  else {
    // don't write to the color buffer, just the stencil buffer
    GL_State(GLS_DEPTHMASK | GLS_COLORMASK | GLS_ALPHAMASK | GLS_DEPTHFUNC_LESS);
  }

  if (r_shadowPolygonFactor.GetFloat() || r_shadowPolygonOffset.GetFloat()) {
    glPolygonOffset(r_shadowPolygonFactor.GetFloat(), -r_shadowPolygonOffset.GetFloat());
    glEnable(GL_POLYGON_OFFSET_FILL);
  }

  glStencilFunc(GL_ALWAYS, 1, 255);

  if (glConfig.depthBoundsTestAvailable && r_useDepthBoundsTest.GetBool()) {
    glEnable(GL_DEPTH_BOUNDS_TEST_EXT);
  }

  RB_RenderDrawSurfChainWithFunction(drawSurfs, RB_GLSL_Shadow);

  GL_Cull(CT_FRONT_SIDED);

  if (r_shadowPolygonFactor.GetFloat() || r_shadowPolygonOffset.GetFloat()) {
    glDisable(GL_POLYGON_OFFSET_FILL);
  }

  if (glConfig.depthBoundsTestAvailable && r_useDepthBoundsTest.GetBool()) {
    glDisable(GL_DEPTH_BOUNDS_TEST_EXT);
  }

  glStencilFunc(GL_GEQUAL, 128, 255);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

  GL_UseProgram(nullptr);
}



/*
==================
RB_GLSL_FillDepthBuffer
==================
*/
void RB_GLSL_FillDepthBuffer(const drawSurf_t *surf) {
  int			stage;
  const idMaterial	*shader;
  const shaderStage_t *pStage;
  const float	*regs;
  float		color[4];
  const srfTriangles_t	*tri;

  tri = surf->geo;
  shader = surf->material;

  // update the clip plane if needed
  if (backEnd.viewDef->numClipPlanes && surf->space != backEnd.currentSpace) {
    GL_SelectTexture(1);

    idPlane	plane;

    R_GlobalPlaneToLocal(surf->space->modelMatrix, backEnd.viewDef->clipPlanes[0], plane);
    plane[3] += 0.5;	// the notch is in the middle
    glTexGenfv(GL_S, GL_OBJECT_PLANE, plane.ToFloatPtr());
    GL_SelectTexture(0);
  }

  if (!shader->IsDrawn()) {
    return;
  }

  // some deforms may disable themselves by setting numIndexes = 0
  if (!tri->numIndexes) {
    return;
  }

  // translucent surfaces don't put anything in the depth buffer and don't
  // test against it, which makes them fail the mirror clip plane operation
  if (shader->Coverage() == MC_TRANSLUCENT) {
    return;
  }

  if (!tri->ambientCache) {
    common->Printf("RB_T_FillDepthBuffer: !tri->ambientCache\n");
    return;
  }

  // get the expressions for conditionals / color / texcoords
  regs = surf->shaderRegisters;

  // if all stages of a material have been conditioned off, don't do anything
  for (stage = 0; stage < shader->GetNumStages(); stage++) {
    pStage = shader->GetStage(stage);
    // check the stage enable condition
    if (regs[pStage->conditionRegister] != 0) {
      break;
    }
  }
  if (stage == shader->GetNumStages()) {
    return;
  }

  // set polygon offset if necessary
  if (shader->TestMaterialFlag(MF_POLYGONOFFSET)) {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * shader->GetPolygonOffset());
  }

  // subviews will just down-modulate the color buffer by overbright
  if (shader->GetSort() == SS_SUBVIEW) {
    GL_State(GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO | GLS_DEPTHFUNC_LESS);
    color[0] =
      color[1] =
      color[2] = (1.0 / backEnd.overBright);
    color[3] = 1;
  }
  else {
    // others just draw black
    color[0] = 0;
    color[1] = 0;
    color[2] = 0;
    color[3] = 1;
  }

  const auto offset = vertexCache.Bind(tri->ambientCache);
  glVertexAttribPointer(glslProgramDef_t::vertex_attrib_position, 3, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::xyzOffset));
  glVertexAttribPointer(glslProgramDef_t::vertex_attrib_texcoord, 2, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::texcoordOffset));
  glUniformMatrix4fv(glslProgramDef_t::uniform_modelViewMatrix, 1, false, GL_ModelViewMatrix.Top());
  glUniformMatrix4fv(glslProgramDef_t::uniform_projectionMatrix, 1, false, GL_ProjectionMatrix.Top());

  bool drawSolid = false;

  if (shader->Coverage() == MC_OPAQUE) {
    drawSolid = true;
  }

  // we may have multiple alpha tested stages
  if (shader->Coverage() == MC_PERFORATED) {
    // if the only alpha tested stages are condition register omitted,
    // draw a normal opaque surface
    bool	didDraw = false;

    // perforated surfaces may have multiple alpha tested stages
    for (stage = 0; stage < shader->GetNumStages(); stage++) {
      pStage = shader->GetStage(stage);

      if (!pStage->hasAlphaTest) {
        continue;
      }

      // check the stage enable condition
      if (regs[pStage->conditionRegister] == 0) {
        continue;
      }

      // if we at least tried to draw an alpha tested stage,
      // we won't draw the opaque surface
      didDraw = true;

      // set the alpha modulate
      color[3] = regs[pStage->color.registers[3]];

      // skip the entire stage if alpha would be black
      if (color[3] <= 0) {
        continue;
      }

      // bind the texture
      pStage->texture.image->Bind();
      
      glUniform1i(glslProgramDef_t::uniform_alphaTestEnabled, 1);
      glUniform1f(glslProgramDef_t::uniform_alphaTestThreshold, regs[pStage->alphaTestRegister]);
      glUniform4fv(glslProgramDef_t::uniform_diffuse_color, 1, color);

      // set texture matrix and texGens      

      if (pStage->privatePolygonOffset && !surf->material->TestMaterialFlag(MF_POLYGONOFFSET)) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * pStage->privatePolygonOffset);
      }

      if (pStage->texture.hasMatrix) {
        idVec4 textureMatrix[2];
        RB_GetShaderTextureMatrix(surf->shaderRegisters, &pStage->texture, textureMatrix);
        glUniform4fv(glslProgramDef_t::uniform_diffuseMatrixS, 1, textureMatrix[0].ToFloatPtr());
        glUniform4fv(glslProgramDef_t::uniform_diffuseMatrixT, 1, textureMatrix[1].ToFloatPtr());
      }
      

      // draw it
      RB_DrawElementsWithCounters(tri);

      //RB_FinishStageTexturing(pStage, surf, ac);
      {
        if (pStage->privatePolygonOffset && !surf->material->TestMaterialFlag(MF_POLYGONOFFSET)) {
          glDisable(GL_POLYGON_OFFSET_FILL);
        }

        idVec4 textureMatrix[2];
        textureMatrix[0][0] = 1;
        textureMatrix[0][1] = 0;
        textureMatrix[0][2] = 0;
        textureMatrix[0][3] = 0;
        textureMatrix[1][0] = 0;
        textureMatrix[1][1] = 1;
        textureMatrix[1][2] = 0;
        textureMatrix[1][3] = 0;
        glUniform4fv(glslProgramDef_t::uniform_diffuseMatrixS, 1, textureMatrix[0].ToFloatPtr());
        glUniform4fv(glslProgramDef_t::uniform_diffuseMatrixT, 1, textureMatrix[1].ToFloatPtr());

        glUniform1i(glslProgramDef_t::uniform_alphaTestEnabled, 0);
      }
    }
    //glDisable(GL_ALPHA_TEST);
    if (!didDraw) {
      drawSolid = true;
    }
  }

  // draw the entire surface solid
  if (drawSolid) {
    glUniform4fv(glslProgramDef_t::uniform_diffuse_color, 1, color);
    //    glColor4fv(color);
    globalImages->whiteImage->Bind();

    // draw it
    RB_DrawElementsWithCounters(tri);
  }


  // reset polygon offset
  if (shader->TestMaterialFlag(MF_POLYGONOFFSET)) {
    glDisable(GL_POLYGON_OFFSET_FILL);
  }

  // reset blending
  if (shader->GetSort() == SS_SUBVIEW) {
    GL_State(GLS_DEPTHFUNC_LESS);
  }

}


/*
=====================
RB_GLSL_FillDepthBuffer

If we are rendering a subview with a near clip plane, use a second texture
to force the alpha test to fail when behind that clip plane
=====================
*/
void RB_GLSL_FillDepthBuffer(drawSurf_t **drawSurfs, int numDrawSurfs) {
  assert(depthProgram);

  // if we are just doing 2D rendering, no need to fill the depth buffer
  if (!backEnd.viewDef->viewEntitys) {
    return;
  }

  RB_LogComment("---------- RB_GLSL_FillDepthBuffer ----------\n");

  // enable the second texture for mirror plane clipping if needed
  if (backEnd.viewDef->numClipPlanes) {
    GL_SelectTexture(1);
    globalImages->alphaNotchImage->Bind();
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnable(GL_TEXTURE_GEN_S);
    glTexCoord2f(1, 0.5);
  }

  // the first texture will be used for alpha tested surfaces
  GL_SelectTexture(0);

  // decal surfaces may enable polygon offset
  glPolygonOffset(r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat());

  GL_State(GLS_DEPTHFUNC_LESS);

  // Enable stencil test if we are going to be using it for shadows.
  // If we didn't do this, it would be legal behavior to get z fighting
  // from the ambient pass and the light passes.
  glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_ALWAYS, 1, 255);

  GL_UseProgram(depthProgram);
  glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
  glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);

  RB_RenderDrawSurfListWithFunction(drawSurfs, numDrawSurfs, RB_GLSL_FillDepthBuffer);

  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);
  GL_UseProgram(nullptr);

  if (backEnd.viewDef->numClipPlanes) {
    GL_SelectTexture(1);
    globalImages->BindNull();
    glDisable(GL_TEXTURE_GEN_S);
    GL_SelectTexture(0);
  }

}



/*
=====================
RB_GLSL_RenderSpecialShaderStage
=====================
*/
void RB_GLSL_RenderSpecialShaderStage(const float* regs, const shaderStage_t* pStage, glslShaderStage_t* glslStage, const srfTriangles_t	*tri) {

  const auto offset = vertexCache.Bind(tri->ambientCache);

  glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
  glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);
  glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_normal);
  glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);
  glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_binormal);
  glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_tangent);

  glVertexAttribPointer(glslProgramDef_t::vertex_attrib_position, 3, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::xyzOffset));
  glVertexAttribPointer(glslProgramDef_t::vertex_attrib_texcoord, 2, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::texcoordOffset));
  glVertexAttribPointer(glslProgramDef_t::vertex_attrib_normal, 3, GL_FLOAT, false, sizeof(idDrawVert),   attributeOffset(offset, idDrawVert::normalOffset));
  glVertexAttribPointer(glslProgramDef_t::vertex_attrib_color, 4, GL_UNSIGNED_BYTE, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::colorOffset));
  glVertexAttribPointer(glslProgramDef_t::vertex_attrib_binormal, 3, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::binormalOffset));
  glVertexAttribPointer(glslProgramDef_t::vertex_attrib_tangent, 3, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::tangentOffset));

  GL_State(pStage->drawStateBits);
  GL_UseProgram(glslStage->program);

  for (int i = 0; i < glslStage->numShaderParms; i++) {
    float	parm[4];
    parm[0] = regs[glslStage->shaderParms[i][0]];
    parm[1] = regs[glslStage->shaderParms[i][1]];
    parm[2] = regs[glslStage->shaderParms[i][2]];
    parm[3] = regs[glslStage->shaderParms[i][3]];
    glUniform4fv(glslProgramDef_t::uniform_shaderparm0 + i, 1, parm);
  }

//  glUniformMatrix4fv(glslProgramDef_t::uniform_modelMatrix, 1, false, surf->space->modelMatrix);
  glUniformMatrix4fv(glslProgramDef_t::uniform_modelViewMatrix, 1, false, GL_ModelViewMatrix.Top());
  glUniformMatrix4fv(glslProgramDef_t::uniform_projectionMatrix, 1, false, GL_ProjectionMatrix.Top());

  // current render
  const int	w = backEnd.viewDef->viewport.x2 - backEnd.viewDef->viewport.x1 + 1;
  const int	h = backEnd.viewDef->viewport.y2 - backEnd.viewDef->viewport.y1 + 1;
  float currentRenderSize[4];
  currentRenderSize[0] = globalImages->currentRenderImage->uploadWidth;
  currentRenderSize[1] = globalImages->currentRenderImage->uploadHeight;
  currentRenderSize[2] = w;
  currentRenderSize[3] = h;
  glUniform4fv(glslProgramDef_t::uniform_currentRenderSize, 1, currentRenderSize);

  // set textures
  for (int i = 0; i < glslStage->numShaderMaps; i++) {
    if (glslStage->shaderMap[i]) {
      GL_SelectTexture(i);
      glslStage->shaderMap[i]->Bind();
    }
  }

  // draw it
  RB_DrawElementsWithCounters(tri);

  for (int i = 0; i < glslStage->numShaderMaps; i++) {
    if (glslStage->shaderMap[i]) {
      GL_SelectTexture(i);
      globalImages->BindNull();
    }
  }

  GL_UseProgram(nullptr);
  GL_SelectTexture(0);

  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);
  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_normal);
  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);
  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_binormal);
  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_tangent);
}


/*
=================
R_PreprocessShader
=================
*/
static bool R_PreprocessShader(char* src, int srcsize, char* dest, int destsize) {
  static const char* const inc_stmt = "#include ";

  char* inc_start = strstr(src, inc_stmt);
  if(!inc_start) {
    if(srcsize >= destsize) {
      common->Warning(": File too large\n");
      return false;
    }     

    memcpy(dest, src, srcsize);
    dest[srcsize] = '\0';

    return true;
  }

  char* filename_start = strstr(inc_start, "\"");
  if(!filename_start)
    return false;
  
  filename_start++;

  char* filename_stop = strstr(filename_start, "\"");
  if(!filename_stop)
    return false;  

  int filename_len = (ptrdiff_t)filename_stop - (ptrdiff_t)filename_start;
  filename_stop++;

  int bytes_before_inc = (ptrdiff_t)inc_start - (ptrdiff_t)src;
  int bytes_after_inc = (ptrdiff_t)srcsize - ((ptrdiff_t)filename_stop - (ptrdiff_t)src);
  
  idStr fullPath = idStr("glsl/") + idStr(filename_start, 0, filename_len);

  char	*fileBuffer = nullptr;
  fileSystem->ReadFile(fullPath.c_str(), (void **)&fileBuffer, NULL);
  if (!fileBuffer) {
    common->Printf(": File not found\n");
    return false;
  }
  int file_size = strlen(fileBuffer);

  if(file_size + bytes_before_inc + bytes_after_inc >= destsize) {
    common->Printf(": File too large\n");
    fileSystem->FreeFile(fileBuffer);
    return false;
  }

  memcpy(dest, src, bytes_before_inc);
  dest += bytes_before_inc;
  memcpy(dest, fileBuffer, file_size);
  dest += file_size;
  memcpy(dest, filename_stop, bytes_after_inc);
  dest[bytes_after_inc] = '\0';
  return true;
}


/*
=================
R_LoadGlslShader
=================
*/
static GLuint R_LoadGlslShader(GLenum shaderType, const char* filename) {
  assert(filename);
  assert(filename[0]);
  assert(shaderType == GL_VERTEX_SHADER || shaderType == GL_FRAGMENT_SHADER);

  idStr	fullPath = "glsl/";
  fullPath += filename;

  if(shaderType == GL_VERTEX_SHADER)
    common->Printf("load vertex shader %s\n", fullPath.c_str());
  else
    common->Printf("load fragment shader %s\n", fullPath.c_str());

  // load the program even if we don't support it, so
  // fs_copyfiles can generate cross-platform data dumps
  // 
  char	*fileBuffer = nullptr;
  fileSystem->ReadFile(fullPath.c_str(), (void **)&fileBuffer, NULL);
  if (!fileBuffer) {
    common->Printf(": File not found\n");
    return 0;
  }

  const int buffer_size = 1024 * 256;
  char* buffer = new char[buffer_size];

  bool ok = R_PreprocessShader(fileBuffer, strlen(fileBuffer), buffer, buffer_size);

  fileSystem->FreeFile(fileBuffer);

  if(!ok) {
    return 0;
  }

  GLuint shaderObject = glCreateShader(shaderType);

  const int bufferLen = strlen(buffer);

  glShaderSource(shaderObject, 1, (const GLchar**)&buffer, &bufferLen);
  glCompileShader(shaderObject);

  delete buffer;

  GLint success = 0;
  glGetShaderiv(shaderObject, GL_COMPILE_STATUS, &success);
  if(success == GL_FALSE) {
    char buffer[1024];
    GLsizei length;
    glGetShaderInfoLog(shaderObject, sizeof(buffer)-1, &length, &buffer[0]);
    buffer[length] = '\0';


    common->Printf("failed to compile shader '%s': %s", filename, &buffer[0]);
    return 0;
  }

  return shaderObject;
}

namespace {
  // Little RAII helper to deal with detaching and deleting of shader objects
  struct GlslShader {
    GlslShader(GLuint ident)
      : ident(ident)
      , program(0)
    {}

    ~GlslShader() {      
      release();        
    }

    void release() {
      if (ident) {
        if (program) {
          glDetachShader(program, ident);
        }
        glDeleteShader(ident);
      }

      ident = 0;
      program = 0;
    }

    void attachToProgram(GLuint program) {
      this->program = program;
      glAttachShader(program, ident);
    }

    GLuint ident;
    GLuint program;
  };
}

/*
=================
R_LoadGlslProgram
=================
*/
static void R_LoadGlslProgram(glslProgramDef_t& programDef) {
  if(!programDef.vertexShaderName[0] || !programDef.fragmentShaderName[0])
    return;

  GlslShader vertexShader = R_LoadGlslShader(GL_VERTEX_SHADER, programDef.vertexShaderName);
  if (!vertexShader.ident) {
    common->Warning("failed to load GLSL vertex shader: %s", programDef.vertexShaderName);
    return;
  }

  GlslShader fragmentShader = R_LoadGlslShader(GL_FRAGMENT_SHADER, programDef.fragmentShaderName);
  if (!fragmentShader.ident) {
    common->Warning("failed to load GLSL fragment shader: %s", programDef.fragmentShaderName);
    return;
  }

  const GLuint program = glCreateProgram();
  if (!program) {
    common->Warning("failed to create GLSL program object");
    return;
  }

  vertexShader.attachToProgram(program);
  fragmentShader.attachToProgram(program);
  glLinkProgram(program);

  GLint isLinked = 0;
  glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
  if (isLinked == GL_FALSE) {

    char buffer[1024];
    GLsizei length;
    glGetProgramInfoLog(program, sizeof(buffer)-1, &length, &buffer[0]);
    buffer[length] = '\0';

    vertexShader.release();
    fragmentShader.release();
    glDeleteProgram(program);

    common->Warning("failed to link GLSL shaders to program: %s", &buffer[0]);
    return;
  }


  int numUni = -1;
  glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numUni);
  programDef.usedTextureUnits = 0;
  for(int i=0; i<numUni; ++i) {
    char name[128] = {'\0'};
    int nameLen = 0;
    int size = 0;
    GLenum type = GL_INVALID_ENUM;
    
    glGetActiveUniform(program, i, sizeof(name)-1, &nameLen, &size, &type, name);
    
    if(type == GL_SAMPLER_2D || type == GL_SAMPLER_CUBE) {
      GLuint location = glGetUniformLocation( program, name );
      programDef.usedTextureUnits |= (1 << (location - glslProgramDef_t::uniform_texture0));
    }
  }

  if (programDef.ident)
    glDeleteProgram(programDef.ident);

  programDef.ident = program;
}

/*
=================
R_FindGlslProgram
=================
*/
const glslProgramDef_t* R_FindGlslProgram(const char* vertexShaderName, const char* fragmentShaderName) {
  assert(vertexShaderName && vertexShaderName[0]);
  assert(fragmentShaderName && fragmentShaderName[0]);

  const int vertexShaderNameLen = strlen(vertexShaderName);
  const int fragmentShaderNameLen = strlen(fragmentShaderName);

  int i;
  for (i = 0; i < MAX_GLPROGS; ++i) {
    const int vsLen = strlen(glslPrograms[i].vertexShaderName);
    const int fsLen = strlen(glslPrograms[i].fragmentShaderName);
    
    if(!vsLen || !fsLen)
      break;

    if(vsLen != vertexShaderNameLen || fsLen != fragmentShaderNameLen)
      continue;

    if(idStr::Icmpn(vertexShaderName, glslPrograms[i].vertexShaderName, vsLen) != 0)
      continue;

    if (idStr::Icmpn(fragmentShaderName, glslPrograms[i].fragmentShaderName, fsLen) != 0)
      continue;

    return &glslPrograms[i];
  }

  if(i >= MAX_GLPROGS) {
    common->Error("cannot create GLSL program, maximum number of programs reached");
    return nullptr;
  }

  strncpy(glslPrograms[i].vertexShaderName, vertexShaderName, vertexShaderNameLen);
  strncpy(glslPrograms[i].fragmentShaderName, fragmentShaderName, fragmentShaderNameLen);

  R_LoadGlslProgram(glslPrograms[i]);

  return &glslPrograms[i];
}


/*
=================
R_ReloadGlslPrograms_f
=================
*/
void R_ReloadGlslPrograms_f( const idCmdArgs &args ) {
  common->Printf( "----- R_ReloadGlslPrograms -----\n" );

  for(int i=0; i<MAX_GLPROGS; ++i) {
    R_LoadGlslProgram(glslPrograms[i]);
  }

  common->Printf( "----- R_ReloadGlslPrograms -----\n" );
}


/*
====================
RB_GLSL_RenderShaderStage
====================
*/
void RB_GLSL_RenderShaderStage(const drawSurf_t *surf, const shaderStage_t* pStage) {
  assert(defaultProgram);
  assert(skyboxProgram);
  assert(bumpyEnvProgram);

  // set the color  
  float color[4];
  color[0] = surf->shaderRegisters[pStage->color.registers[0]];
  color[1] = surf->shaderRegisters[pStage->color.registers[1]];
  color[2] = surf->shaderRegisters[pStage->color.registers[2]];
  color[3] = surf->shaderRegisters[pStage->color.registers[3]];

  // skip the entire stage if an add would be black
  if ((pStage->drawStateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) == (GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE)
    && color[0] <= 0 && color[1] <= 0 && color[2] <= 0) {
    return;
  }

  // skip the entire stage if a blend would be completely transparent
  if ((pStage->drawStateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) == (GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA)
    && color[3] <= 0) {
    return;
  }
  
  const auto offset = vertexCache.Bind(surf->geo->ambientCache); 


  if (pStage->texture.texgen == TG_DIFFUSE_CUBE) {
    return;
  }
  else if (pStage->texture.texgen == TG_SKYBOX_CUBE || pStage->texture.texgen == TG_WOBBLESKY_CUBE) {
    GL_UseProgram(skyboxProgram);    

    idMat4 textureMatrix = mat4_identity;
    if (pStage->texture.texgen == TG_WOBBLESKY_CUBE) {
      R_CreateWobbleskyTexMatrix(surf, backEnd.viewDef->floatTime, textureMatrix.ToFloatPtr());
    }

    idVec4 localViewOrigin;
    R_GlobalPointToLocal( surf->space->modelMatrix, backEnd.viewDef->renderView.vieworg, localViewOrigin.ToVec3() );
    localViewOrigin[3] = 1.0f;
    glUniform4fv(glslProgramDef_t::uniform_localViewOrigin, 1, localViewOrigin.ToFloatPtr());
    glUniformMatrix4fv(glslProgramDef_t::uniform_textureMatrix0, 1, false, textureMatrix.ToFloatPtr());

    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);    
    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);

    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_position, 3, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::xyzOffset));
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_color, 4, GL_UNSIGNED_BYTE, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::colorOffset));    
  }
  else if (pStage->texture.texgen == TG_SCREEN) {
    return;
  }
  else if (pStage->texture.texgen == TG_GLASSWARP) {
    return;
  }
  else if (pStage->texture.texgen == TG_REFLECT_CUBE) {

    GL_UseProgram(bumpyEnvProgram);

    idMat4 textureMatrix = mat4_identity;

    idVec4 localViewOrigin;
    R_GlobalPointToLocal(surf->space->modelMatrix, backEnd.viewDef->renderView.vieworg, localViewOrigin.ToVec3());
    localViewOrigin[3] = 1.0f;
    glUniform4fv(glslProgramDef_t::uniform_localViewOrigin, 1, localViewOrigin.ToFloatPtr());
    glUniformMatrix4fv(glslProgramDef_t::uniform_textureMatrix0, 1, false, textureMatrix.ToFloatPtr());

    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);
    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_normal);
    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);
    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_binormal);
    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_tangent);

    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_position, 3, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::xyzOffset));
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_texcoord, 2, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::texcoordOffset));
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_normal, 3, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::normalOffset));
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_color, 4, GL_UNSIGNED_BYTE, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::colorOffset));
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_binormal, 3, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::binormalOffset));
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_tangent, 3, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::tangentOffset));

    // set the texture matrix
    idVec4 textureMatrixST[2];
    textureMatrixST[0][0] = 1;
    textureMatrixST[0][1] = 0;
    textureMatrixST[0][2] = 0;
    textureMatrixST[0][3] = 0;
    textureMatrixST[1][0] = 0;
    textureMatrixST[1][1] = 1;
    textureMatrixST[1][2] = 0;
    textureMatrixST[1][3] = 0;

    // see if there is also a bump map specified
    GL_SelectTextureNoClient(2);
    if(const shaderStage_t *bumpStage = surf->material->GetBumpStage()) {
      RB_GetShaderTextureMatrix(surf->shaderRegisters, &bumpStage->texture, textureMatrixST);

      //void RB_GetShaderTextureMatrix( const float *shaderRegisters, const textureStage_t *texture, idVec4 matrix[2] );
      bumpStage->texture.image->Bind();
    } else {
      globalImages->flatNormalMap->Bind();
    }
    GL_SelectTextureNoClient(0);

    glUniform4fv(glslProgramDef_t::uniform_bumpMatrixS, 1, textureMatrixST[0].ToFloatPtr());
    glUniform4fv(glslProgramDef_t::uniform_bumpMatrixT, 1, textureMatrixST[1].ToFloatPtr());

  }
  else {    

    //prefer depth blend settings from material. If material does not define a 
    // depth blend mode, look at the geometry for depth blend settings (usually
    // set by particle systems for soft particles)

    depthBlendMode_t depthBlendMode = pStage->depthBlendMode;
    float depthBlendRange = pStage->depthBlendRange;

    if(depthBlendMode == DBM_UNDEFINED) {
      depthBlendMode = surf->geo->depthBlendMode;
      depthBlendRange = surf->geo->depthBlendRange;
    }      
    
    if (depthBlendMode == DBM_AUTO) {
      if (pStage->drawStateBits & (GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE))
        depthBlendMode = DBM_COLORALPHA_ZERO;
      else if (pStage->drawStateBits & (GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO))
        depthBlendMode = DBM_COLORALPHA_ONE;
      else
        depthBlendMode = DBM_OFF;
    }

    if(depthBlendMode != DBM_OFF && depthBlendRange > 0.0f) {
      GL_UseProgram(depthblendProgram);
      GL_SelectTexture(7);
      globalImages->currentDepthImage->Bind();

      glUniform1f(glslProgramDef_t::uniform_depthBlendRange, depthBlendRange);
      glUniform1i(glslProgramDef_t::uniform_depthBlendMode, static_cast<int>(depthBlendMode));

      float clipRange[] = { backEnd.viewDef->viewFrustum.GetNearDistance(), backEnd.viewDef->viewFrustum.GetFarDistance() };
      glUniform2fv(glslProgramDef_t::uniform_clipRange, 1, clipRange);
    }
    else {
      GL_UseProgram(defaultProgram);
    }

    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);
    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);

    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_position, 3, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::xyzOffset));
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_texcoord, 2, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::texcoordOffset));
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_color, 4, GL_UNSIGNED_BYTE, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::colorOffset));
  }

  GL_SelectTexture(1);

  // bind the texture
  RB_BindVariableStageImage(&pStage->texture, surf->shaderRegisters);
  GL_SelectTexture(0);

  // set the state
  GL_State(pStage->drawStateBits);

  static const float zero[4] = { 0, 0, 0, 0 };
  static const float one[4] = { 1, 1, 1, 1 };
  static const float negOne[4] = { -1, -1, -1, -1 };

  switch (pStage->vertexColor) {
  case SVC_IGNORE:
    glUniform4fv(glslProgramDef_t::uniform_color_modulate, 1, zero);
    glUniform4fv(glslProgramDef_t::uniform_color_add, 1, one);
    break;
  case SVC_MODULATE:
    glUniform4fv(glslProgramDef_t::uniform_color_modulate, 1, one);
    glUniform4fv(glslProgramDef_t::uniform_color_add, 1, zero);
    break;
  case SVC_INVERSE_MODULATE:
    glUniform4fv(glslProgramDef_t::uniform_color_modulate, 1, negOne);
    glUniform4fv(glslProgramDef_t::uniform_color_add, 1, one);
    break;
  }  

  glUniformMatrix4fv(glslProgramDef_t::uniform_modelMatrix, 1, false, surf->space->modelMatrix);
  glUniformMatrix4fv(glslProgramDef_t::uniform_modelViewMatrix, 1, false, GL_ModelViewMatrix.Top());
  glUniformMatrix4fv(glslProgramDef_t::uniform_projectionMatrix, 1, false, GL_ProjectionMatrix.Top());

  // current render
  const int	w = backEnd.viewDef->viewport.x2 - backEnd.viewDef->viewport.x1 + 1;
  const int	h = backEnd.viewDef->viewport.y2 - backEnd.viewDef->viewport.y1 + 1;
  float currentRenderSize[4];
  currentRenderSize[0] = globalImages->currentRenderImage->uploadWidth;
  currentRenderSize[1] = globalImages->currentRenderImage->uploadHeight;
  currentRenderSize[2] = w;
  currentRenderSize[3] = h;
  glUniform4fv(glslProgramDef_t::uniform_currentRenderSize, 1, currentRenderSize);

  // set privatePolygonOffset if necessary
  if (pStage->privatePolygonOffset) {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * pStage->privatePolygonOffset);
  }

  if(pStage->texture.image) {
    // set the texture matrix
    idVec4 textureMatrix[2];
    textureMatrix[0][0] = 1;
    textureMatrix[0][1] = 0;
    textureMatrix[0][2] = 0;
    textureMatrix[0][3] = 0;
    textureMatrix[1][0] = 0;
    textureMatrix[1][1] = 1;
    textureMatrix[1][2] = 0;
    textureMatrix[1][3] = 0;
    if(pStage->texture.hasMatrix)
      RB_GetShaderTextureMatrix(surf->shaderRegisters, &pStage->texture, textureMatrix);
    glUniform4fv(glslProgramDef_t::uniform_bumpMatrixS, 1, textureMatrix[0].ToFloatPtr());
    glUniform4fv(glslProgramDef_t::uniform_bumpMatrixT, 1, textureMatrix[1].ToFloatPtr());    
  
    GL_SelectTextureNoClient(1);
    pStage->texture.image->Bind();
    GL_SelectTextureNoClient(0);
  }

  glUniform4fv(glslProgramDef_t::uniform_diffuse_color, 1, color);

  // draw it
  RB_DrawElementsWithCounters(surf->geo);

  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);
  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_normal);
  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);
  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_binormal);
  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_tangent);
  GL_UseProgram(nullptr);
}


/*
==================
RB_GLSL_DrawInteraction
==================
*/
void	RB_GLSL_DrawInteraction(const drawInteraction_t *din) {  

  glUniformMatrix4fv(glslProgramDef_t::uniform_modelMatrix, 1, false, din->surf->space->modelMatrix);
  glUniformMatrix4fv(glslProgramDef_t::uniform_modelViewMatrix, 1, false, GL_ModelViewMatrix.Top());
  glUniformMatrix4fv(glslProgramDef_t::uniform_projectionMatrix, 1, false, GL_ProjectionMatrix.Top());

  glUniform4fv(glslProgramDef_t::uniform_localLightOrigin, 1, din->localLightOrigin.ToFloatPtr());
  glUniform4fv(glslProgramDef_t::uniform_localViewOrigin, 1, din->localViewOrigin.ToFloatPtr());
  
  glUniform4fv(glslProgramDef_t::uniform_lightProjectionMatrixS, 1, din->lightProjection[0].ToFloatPtr());
  glUniform4fv(glslProgramDef_t::uniform_lightProjectionMatrixT, 1, din->lightProjection[1].ToFloatPtr());
  glUniform4fv(glslProgramDef_t::uniform_lightProjectionMatrixQ, 1, din->lightProjection[2].ToFloatPtr());
  glUniform4fv(glslProgramDef_t::uniform_lightFallOffS, 1, din->lightProjection[3].ToFloatPtr());

  glUniform4fv(glslProgramDef_t::uniform_bumpMatrixS, 1, din->bumpMatrix[0].ToFloatPtr());
  glUniform4fv(glslProgramDef_t::uniform_bumpMatrixT, 1, din->bumpMatrix[1].ToFloatPtr());
  glUniform4fv(glslProgramDef_t::uniform_diffuseMatrixS, 1, din->diffuseMatrix[0].ToFloatPtr());
  glUniform4fv(glslProgramDef_t::uniform_diffuseMatrixT, 1, din->diffuseMatrix[1].ToFloatPtr());
  glUniform4fv(glslProgramDef_t::uniform_specularMatrixS, 1, din->specularMatrix[0].ToFloatPtr());
  glUniform4fv(glslProgramDef_t::uniform_specularMatrixT, 1, din->specularMatrix[1].ToFloatPtr());

  static const float zero[4] = { 0, 0, 0, 0 };
  static const float one[4] = { 1, 1, 1, 1 };
  static const float negOne[4] = { -1, -1, -1, -1 };

  switch (din->vertexColor) {
  case SVC_IGNORE:
    glUniform4fv(glslProgramDef_t::uniform_color_modulate, 1, zero);
    glUniform4fv(glslProgramDef_t::uniform_color_add, 1, one);
    break;
  case SVC_MODULATE:
    glUniform4fv(glslProgramDef_t::uniform_color_modulate, 1, one);
    glUniform4fv(glslProgramDef_t::uniform_color_add, 1, zero);
    break;
  case SVC_INVERSE_MODULATE:
    glUniform4fv(glslProgramDef_t::uniform_color_modulate, 1, negOne);
    glUniform4fv(glslProgramDef_t::uniform_color_add, 1, one);
    break;
  }

  glUniform4fv(glslProgramDef_t::uniform_diffuse_color, 1, din->diffuseColor.ToFloatPtr());
  glUniform4fv(glslProgramDef_t::uniform_specular_color, 1, din->specularColor.ToFloatPtr());    
  
  // texture 1 will be the per-surface bump map
  GL_SelectTextureNoClient(1);
  din->bumpImage->Bind();

  // texture 2 will be the light falloff texture
  GL_SelectTextureNoClient(2);
  din->lightFalloffImage->Bind();

  // texture 3 will be the light projection texture
  GL_SelectTextureNoClient(3);
  din->lightImage->Bind();

  // texture 4 is the per-surface diffuse map
  GL_SelectTextureNoClient(4);
  din->diffuseImage->Bind();

  // texture 5 is the per-surface specular map
  GL_SelectTextureNoClient(5);
  din->specularImage->Bind();

  // draw it
  RB_DrawElementsWithCounters(din->surf->geo);
}

/*
=============
RB_GLSL_CreateDrawInteractions

=============
*/
void RB_GLSL_CreateDrawInteractions(const drawSurf_t *surf) {
  assert(interactionProgram);

  if (!surf) {
    return;
  }

  // perform setup here that will be constant for all interactions
  GL_State(GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHMASK | backEnd.depthFunc);  

  // bind the vertex program
  //glDisable(GL_VERTEX_PROGRAM_ARB);
  //glDisable(GL_FRAGMENT_PROGRAM_ARB);
  GL_UseProgram(interactionProgram);

  glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
  glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);
  glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_normal);
  glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);
  glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_binormal);
  glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_tangent);

  glUniform1f(glslProgramDef_t::uniform_pomMaxHeight, r_pomEnabled.GetBool() ? r_pomMaxHeight.GetFloat() : -1);
  glUniform1i(glslProgramDef_t::uniform_shading, r_shading.GetInteger());
  glUniform1f(glslProgramDef_t::uniform_specularExp, r_specularExp.GetFloat());
  glUniform1f(glslProgramDef_t::uniform_specularScale, r_specularScale.GetFloat());  

  if (r_ignore.GetBool() && !backEnd.vLight->lightDef->parms.noShadows) {
	  const idVec4 globalLightOrigin = idVec4(backEnd.vLight->globalLightOrigin, 1);
	  glUniform4fv(glslProgramDef_t::uniform_globalLightOrigin, 1, globalLightOrigin.ToFloatPtr());

	  idVec4 shadowParams;
	  int samples;
	  RB_GLSL_GetShadowParams(&shadowParams.x, &shadowParams.y, &shadowParams.z, &samples);
	  shadowParams.w = backEnd.vLight->lightDef->GetMaximumCenterToEdgeDistance();

	  glUniform4fv(glslProgramDef_t::uniform_shadowParams, 1, shadowParams.ToFloatPtr());
	  glUniform1i(glslProgramDef_t::uniform_shadowSamples, samples);

	  glUniformMatrix4fv(glslProgramDef_t::uniform_shadowViewProjection, 6, false, &backEnd.shadowViewProjection[0][0]);
	  glUniform1i(glslProgramDef_t::uniform_shadowMappingMode, 1);
  } else {
	  glUniform1i(glslProgramDef_t::uniform_shadowMappingMode, 0);
  }


  for (; surf; surf = surf->nextOnLight) {
    // perform setup here that will not change over multiple interaction passes

    // set the vertex pointers
    const auto offset = vertexCache.Bind(surf->geo->ambientCache);

    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_position, 3, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::xyzOffset));
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_texcoord, 2, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::texcoordOffset));
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_normal, 3, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::normalOffset));
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_color, 4, GL_UNSIGNED_BYTE, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::colorOffset));
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_binormal, 3, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::binormalOffset));
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_tangent, 3, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::tangentOffset));

    // this may cause RB_ARB2_DrawInteraction to be exacuted multiple
    // times with different colors and images if the surface or light have multiple layers
    RB_CreateSingleDrawInteractions(surf, RB_GLSL_DrawInteraction);
  }

  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);
  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_normal);
  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);
  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_binormal);
  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_tangent);

  // disable features
#if 0
  GL_SelectTextureNoClient(6);
  globalImages->BindNull();
#endif

  GL_SelectTextureNoClient(5);
  globalImages->BindNull();

  GL_SelectTextureNoClient(4);
  globalImages->BindNull();

  GL_SelectTextureNoClient(3);
  globalImages->BindNull();

  GL_SelectTextureNoClient(2);
  globalImages->BindNull();

  GL_SelectTextureNoClient(1);
  globalImages->BindNull();

  backEnd.glState.currenttmu = -1;
  GL_SelectTexture(0);

  GL_UseProgram(nullptr);
}



/*
==================
RB_GLSL_DrawInteractions
==================
*/
void RB_GLSL_DrawInteractions(void) {
  viewLight_t		*vLight;
  const idMaterial	*lightShader;

  //
  // for each light, perform adding and shadowing
  //
  for (vLight = backEnd.viewDef->viewLights; vLight; vLight = vLight->next) {
    backEnd.vLight = vLight;

    // do fogging later
    if (vLight->lightShader->IsFogLight()) {
      continue;
    }
    if (vLight->lightShader->IsBlendLight()) {
      continue;
    }

    if (!vLight->localInteractions && !vLight->globalInteractions
      && !vLight->translucentInteractions) {
      continue;
    }	

	if (r_ignore.GetBool())
	{
		if(!vLight->lightDef->parms.noShadows)
			RB_RenderShadowMaps(vLight);
	}

    lightShader = vLight->lightShader;

    // clear the stencil buffer if needed
    if (vLight->globalShadows || vLight->localShadows) {
      backEnd.currentScissor = vLight->scissorRect;
      if (r_useScissor.GetBool()) {
        glScissor(backEnd.viewDef->viewport.x1 + backEnd.currentScissor.x1,
          backEnd.viewDef->viewport.y1 + backEnd.currentScissor.y1,
          backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1,
          backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1);
      }
      glClear(GL_STENCIL_BUFFER_BIT);
    }
    else {
      // no shadows, so no need to read or write the stencil buffer
      // we might in theory want to use GL_ALWAYS instead of disabling
      // completely, to satisfy the invarience rules
      glStencilFunc(GL_ALWAYS, 128, 255);
    }

	RB_GLSL_StencilShadowPass(vLight->globalShadows);
	RB_GLSL_CreateDrawInteractions(vLight->localInteractions);
	RB_GLSL_StencilShadowPass(vLight->localShadows);
	RB_GLSL_CreateDrawInteractions(vLight->globalInteractions);

    // translucent surfaces never get stencil shadowed
    if (r_skipTranslucent.GetBool()) {
      continue;
    }

    glStencilFunc(GL_ALWAYS, 128, 255);

    backEnd.depthFunc = GLS_DEPTHFUNC_LESS;
    RB_GLSL_CreateDrawInteractions(vLight->translucentInteractions);

    backEnd.depthFunc = GLS_DEPTHFUNC_EQUAL;
  }

  // disable stencil shadow test
  glStencilFunc(GL_ALWAYS, 128, 255);
}


