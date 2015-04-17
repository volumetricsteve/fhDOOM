/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).  

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"



/*
================
RB_PrepareStageTexturing
================
*/
void RB_PrepareStageTexturing(const shaderStage_t *pStage, const drawSurf_t *surf, idDrawVert *ac) {
  // set privatePolygonOffset if necessary
  if (pStage->privatePolygonOffset) {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat() * pStage->privatePolygonOffset);
  }

  // set the texture matrix if needed
  if (pStage->texture.hasMatrix) {
    RB_LoadShaderTextureMatrix(surf->shaderRegisters, &pStage->texture);
  }

  // texgens
  if (pStage->texture.texgen == TG_DIFFUSE_CUBE) {
    glTexCoordPointer(3, GL_FLOAT, sizeof(idDrawVert), ac->normal.ToFloatPtr());
  }
  if (pStage->texture.texgen == TG_SKYBOX_CUBE || pStage->texture.texgen == TG_WOBBLESKY_CUBE) {
    glTexCoordPointer(3, GL_FLOAT, 0, vertexCache.Position(surf->dynamicTexCoords));
  }
  if (pStage->texture.texgen == TG_SCREEN) {
    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);
    glEnable(GL_TEXTURE_GEN_Q);

    float	mat[16], plane[4];
    myGlMultMatrix(surf->space->modelViewMatrix, backEnd.viewDef->projectionMatrix, mat);

    plane[0] = mat[0];
    plane[1] = mat[4];
    plane[2] = mat[8];
    plane[3] = mat[12];
    glTexGenfv(GL_S, GL_OBJECT_PLANE, plane);

    plane[0] = mat[1];
    plane[1] = mat[5];
    plane[2] = mat[9];
    plane[3] = mat[13];
    glTexGenfv(GL_T, GL_OBJECT_PLANE, plane);

    plane[0] = mat[3];
    plane[1] = mat[7];
    plane[2] = mat[11];
    plane[3] = mat[15];
    glTexGenfv(GL_Q, GL_OBJECT_PLANE, plane);
  }

  if (pStage->texture.texgen == TG_SCREEN2) {
    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);
    glEnable(GL_TEXTURE_GEN_Q);

    float	mat[16], plane[4];
    myGlMultMatrix(surf->space->modelViewMatrix, backEnd.viewDef->projectionMatrix, mat);

    plane[0] = mat[0];
    plane[1] = mat[4];
    plane[2] = mat[8];
    plane[3] = mat[12];
    glTexGenfv(GL_S, GL_OBJECT_PLANE, plane);

    plane[0] = mat[1];
    plane[1] = mat[5];
    plane[2] = mat[9];
    plane[3] = mat[13];
    glTexGenfv(GL_T, GL_OBJECT_PLANE, plane);

    plane[0] = mat[3];
    plane[1] = mat[7];
    plane[2] = mat[11];
    plane[3] = mat[15];
    glTexGenfv(GL_Q, GL_OBJECT_PLANE, plane);
  }

  if (pStage->texture.texgen == TG_GLASSWARP) {
    glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, FPROG_GLASSWARP);
    glEnable(GL_FRAGMENT_PROGRAM_ARB);

    GL_SelectTexture(2);
    globalImages->scratchImage->Bind();

    GL_SelectTexture(1);
    globalImages->scratchImage2->Bind();

    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);
    glEnable(GL_TEXTURE_GEN_Q);

    float	mat[16], plane[4];
    myGlMultMatrix(surf->space->modelViewMatrix, backEnd.viewDef->projectionMatrix, mat);

    plane[0] = mat[0];
    plane[1] = mat[4];
    plane[2] = mat[8];
    plane[3] = mat[12];
    glTexGenfv(GL_S, GL_OBJECT_PLANE, plane);

    plane[0] = mat[1];
    plane[1] = mat[5];
    plane[2] = mat[9];
    plane[3] = mat[13];
    glTexGenfv(GL_T, GL_OBJECT_PLANE, plane);

    plane[0] = mat[3];
    plane[1] = mat[7];
    plane[2] = mat[11];
    plane[3] = mat[15];
    glTexGenfv(GL_Q, GL_OBJECT_PLANE, plane);

    GL_SelectTexture(0);
  }

  if (pStage->texture.texgen == TG_REFLECT_CUBE) {
    // see if there is also a bump map specified
    const shaderStage_t *bumpStage = surf->material->GetBumpStage();
    if (bumpStage) {
      // per-pixel reflection mapping with bump mapping
      GL_SelectTexture(1);
      bumpStage->texture.image->Bind();
      GL_SelectTexture(0);

      glNormalPointer(GL_FLOAT, sizeof(idDrawVert), ac->normal.ToFloatPtr());
      glVertexAttribPointerARB(10, 3, GL_FLOAT, false, sizeof(idDrawVert), ac->tangents[1].ToFloatPtr());
      glVertexAttribPointerARB(9, 3, GL_FLOAT, false, sizeof(idDrawVert), ac->tangents[0].ToFloatPtr());

      glEnableVertexAttribArrayARB(9);
      glEnableVertexAttribArrayARB(10);
      glEnableClientState(GL_NORMAL_ARRAY);

      // Program env 5, 6, 7, 8 have been set in RB_SetProgramEnvironmentSpace

      glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, FPROG_BUMPY_ENVIRONMENT);
      glEnable(GL_FRAGMENT_PROGRAM_ARB);
      glBindProgramARB(GL_VERTEX_PROGRAM_ARB, VPROG_BUMPY_ENVIRONMENT);
      glEnable(GL_VERTEX_PROGRAM_ARB);
    }
    else {
      // per-pixel reflection mapping without a normal map
      glNormalPointer(GL_FLOAT, sizeof(idDrawVert), ac->normal.ToFloatPtr());
      glEnableClientState(GL_NORMAL_ARRAY);

      glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, FPROG_ENVIRONMENT);
      glEnable(GL_FRAGMENT_PROGRAM_ARB);
      glBindProgramARB(GL_VERTEX_PROGRAM_ARB, VPROG_ENVIRONMENT);
      glEnable(GL_VERTEX_PROGRAM_ARB);
    }
  }
}

/*
================
RB_FinishStageTexturing
================
*/
void RB_FinishStageTexturing(const shaderStage_t *pStage, const drawSurf_t *surf, idDrawVert *ac) {
  // unset privatePolygonOffset if necessary
  if (pStage->privatePolygonOffset && !surf->material->TestMaterialFlag(MF_POLYGONOFFSET)) {
    glDisable(GL_POLYGON_OFFSET_FILL);
  }

  if (pStage->texture.texgen == TG_DIFFUSE_CUBE || pStage->texture.texgen == TG_SKYBOX_CUBE
    || pStage->texture.texgen == TG_WOBBLESKY_CUBE) {
    glTexCoordPointer(2, GL_FLOAT, sizeof(idDrawVert), (void *)&ac->st);
  }

  if (pStage->texture.texgen == TG_SCREEN) {
    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_GEN_T);
    glDisable(GL_TEXTURE_GEN_Q);
  }
  if (pStage->texture.texgen == TG_SCREEN2) {
    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_GEN_T);
    glDisable(GL_TEXTURE_GEN_Q);
  }

  if (pStage->texture.texgen == TG_GLASSWARP) {
    GL_SelectTexture(2);
    globalImages->BindNull();

    GL_SelectTexture(1);
    if (pStage->texture.hasMatrix) {
      RB_LoadShaderTextureMatrix(surf->shaderRegisters, &pStage->texture);
    }
    glDisable(GL_TEXTURE_GEN_S);
    glDisable(GL_TEXTURE_GEN_T);
    glDisable(GL_TEXTURE_GEN_Q);
    glDisable(GL_FRAGMENT_PROGRAM_ARB);
    globalImages->BindNull();
    GL_SelectTexture(0);
  }

  if (pStage->texture.texgen == TG_REFLECT_CUBE) {
    // see if there is also a bump map specified
    const shaderStage_t *bumpStage = surf->material->GetBumpStage();
    if (bumpStage) {
      // per-pixel reflection mapping with bump mapping
      GL_SelectTexture(1);
      globalImages->BindNull();
      GL_SelectTexture(0);

      glDisableVertexAttribArrayARB(9);
      glDisableVertexAttribArrayARB(10);
    }
    else {
      // per-pixel reflection mapping without bump mapping
    }

    glDisableClientState(GL_NORMAL_ARRAY);
    glDisable(GL_FRAGMENT_PROGRAM_ARB);
    glDisable(GL_VERTEX_PROGRAM_ARB);
    // Fixme: Hack to get around an apparent bug in ATI drivers.  Should remove as soon as it gets fixed.
    glBindProgramARB(GL_VERTEX_PROGRAM_ARB, 0);
  }

  if (pStage->texture.hasMatrix) {
    GL_TextureMatrix.LoadIdentity();
  }
}


/*
=============================================================================================

FILL DEPTH BUFFER

=============================================================================================
*/


/*
==================
RB_T_FillDepthBuffer
==================
*/
void RB_T_FillDepthBuffer(const drawSurf_t *surf) {
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

  idDrawVert *ac = (idDrawVert *)vertexCache.Position(tri->ambientCache);
  glVertexPointer(3, GL_FLOAT, sizeof(idDrawVert), ac->xyz.ToFloatPtr());
  glTexCoordPointer(2, GL_FLOAT, sizeof(idDrawVert), reinterpret_cast<void *>(&ac->st));

  bool drawSolid = false;

  if (shader->Coverage() == MC_OPAQUE) {
    drawSolid = true;
  }

  // we may have multiple alpha tested stages
  if (shader->Coverage() == MC_PERFORATED) {
    // if the only alpha tested stages are condition register omitted,
    // draw a normal opaque surface
    bool	didDraw = false;

    glEnable(GL_ALPHA_TEST);
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
      glColor4fv(color);

      glAlphaFunc(GL_GREATER, regs[pStage->alphaTestRegister]);

      // bind the texture
      pStage->texture.image->Bind();

      // set texture matrix and texGens
      RB_PrepareStageTexturing(pStage, surf, ac);

      // draw it
      RB_DrawElementsWithCounters(tri);

      RB_FinishStageTexturing(pStage, surf, ac);
    }
    glDisable(GL_ALPHA_TEST);
    if (!didDraw) {
      drawSolid = true;
    }
  }

  // draw the entire surface solid
  if (drawSolid) {
    glColor4fv(color);
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
RB_STD_FillDepthBuffer

If we are rendering a subview with a near clip plane, use a second texture
to force the alpha test to fail when behind that clip plane
=====================
*/
void RB_STD_FillDepthBuffer(drawSurf_t **drawSurfs, int numDrawSurfs) {
  // if we are just doing 2D rendering, no need to fill the depth buffer
  if (!backEnd.viewDef->viewEntitys) {
    return;
  }

  RB_LogComment("---------- RB_STD_FillDepthBuffer ----------\n");

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
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);

  // decal surfaces may enable polygon offset
  glPolygonOffset(r_offsetFactor.GetFloat(), r_offsetUnits.GetFloat());

  GL_State(GLS_DEPTHFUNC_LESS);

  // Enable stencil test if we are going to be using it for shadows.
  // If we didn't do this, it would be legal behavior to get z fighting
  // from the ambient pass and the light passes.
  glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_ALWAYS, 1, 255);

  RB_RenderDrawSurfListWithFunction(drawSurfs, numDrawSurfs, RB_T_FillDepthBuffer);

  if (backEnd.viewDef->numClipPlanes) {
    GL_SelectTexture(1);
    globalImages->BindNull();
    glDisable(GL_TEXTURE_GEN_S);
    GL_SelectTexture(0);
  }
}


/*
==============================================================================

BACK END RENDERING OF STENCIL SHADOWS

==============================================================================
*/
/*
=====================
RB_T_Shadow

the shadow volumes face INSIDE
=====================
*/


static void RB_T_Shadow(const drawSurf_t *surf) {
  const srfTriangles_t  *tri;

  // set the light position if we are using a vertex program to project the rear surfaces
  if (surf->space != backEnd.currentSpace) {
    idVec4 localLight;

    R_GlobalPointToLocal(surf->space->modelMatrix, backEnd.vLight->globalLightOrigin, localLight.ToVec3());
    localLight.w = 0.0f;
    glProgramEnvParameter4fvARB(GL_VERTEX_PROGRAM_ARB, PP_LIGHT_ORIGIN, localLight.ToFloatPtr());
  }

  tri = surf->geo;

  if (!tri->shadowCache) {
    return;
  }

  glVertexPointer(4, GL_FLOAT, sizeof(shadowCache_t), vertexCache.Position(tri->shadowCache));

  // we always draw the sil planes, but we may not need to draw the front or rear caps
  int numIndexes;
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
}


/*
=====================
RB_StencilShadowPass

Stencil test should already be enabled, and the stencil buffer should have
been set to 128 on any surfaces that might receive shadows
=====================
*/
void RB_StencilShadowPass(const drawSurf_t *drawSurfs) {

  glEnable(GL_VERTEX_PROGRAM_ARB);
  glBindProgramARB(GL_VERTEX_PROGRAM_ARB, VPROG_STENCIL_SHADOW);

  if (!r_shadows.GetBool()) {
    return;
  }

  if (!drawSurfs) {
    return;
  }

  RB_LogComment("---------- RB_StencilShadowPass ----------\n");

  globalImages->BindNull();
  glDisableClientState(GL_TEXTURE_COORD_ARRAY);

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

  RB_RenderDrawSurfChainWithFunction(drawSurfs, RB_T_Shadow);

  GL_Cull(CT_FRONT_SIDED);

  if (r_shadowPolygonFactor.GetFloat() || r_shadowPolygonOffset.GetFloat()) {
    glDisable(GL_POLYGON_OFFSET_FILL);
  }

  if (glConfig.depthBoundsTestAvailable && r_useDepthBoundsTest.GetBool()) {
    glDisable(GL_DEPTH_BOUNDS_TEST_EXT);
  }

  glEnableClientState(GL_TEXTURE_COORD_ARRAY);

  glStencilFunc(GL_GEQUAL, 128, 255);
  glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
}


/*
=========================================================================================

GENERAL INTERACTION RENDERING

=========================================================================================
*/

/*
====================
GL_SelectTextureNoClient
====================
*/
static void GL_SelectTextureNoClient( int unit ) {
	backEnd.glState.currenttmu = unit;
	glActiveTextureARB( GL_TEXTURE0_ARB + unit );
	RB_LogComment( "glActiveTextureARB( %i )\n", unit );
}

/*
====================
RB_STD_RenderShaderStage
====================
*/
void RB_STD_RenderShaderStage(const drawSurf_t *surf, const shaderStage_t* pStage) {

  const srfTriangles_t* const tri = surf->geo;

  idDrawVert *ac = (idDrawVert *)vertexCache.Position(tri->ambientCache);
  glVertexPointer(3, GL_FLOAT, sizeof(idDrawVert), ac->xyz.ToFloatPtr());
  glTexCoordPointer(2, GL_FLOAT, sizeof(idDrawVert), reinterpret_cast<void *>(&ac->st));

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

  // select the vertex color source
  if (pStage->vertexColor == SVC_IGNORE) {
    glColor4fv(color);
  }
  else {
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(idDrawVert), (void *)&ac->color);
    glEnableClientState(GL_COLOR_ARRAY);

    if (pStage->vertexColor == SVC_INVERSE_MODULATE) {
      GL_TexEnv(GL_COMBINE_ARB);
      glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
      glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_TEXTURE);
      glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_PRIMARY_COLOR_ARB);
      glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
      glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_ONE_MINUS_SRC_COLOR);
      glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1);
    }

    // for vertex color and modulated color, we need to enable a second
    // texture stage
    if (color[0] != 1 || color[1] != 1 || color[2] != 1 || color[3] != 1) {
      GL_SelectTexture(1);

      globalImages->whiteImage->Bind();
      GL_TexEnv(GL_COMBINE_ARB);

      glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, color);

      glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_ARB, GL_MODULATE);
      glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_ARB, GL_PREVIOUS_ARB);
      glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_ARB, GL_CONSTANT_ARB);
      glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB_ARB, GL_SRC_COLOR);
      glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_ARB, GL_SRC_COLOR);
      glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE_ARB, 1);

      glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_ARB, GL_MODULATE);
      glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_ARB, GL_PREVIOUS_ARB);
      glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_ARB, GL_CONSTANT_ARB);
      glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA_ARB, GL_SRC_ALPHA);
      glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA_ARB, GL_SRC_ALPHA);
      glTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1);

      GL_SelectTexture(0);
    }
  }

  // bind the texture
  RB_BindVariableStageImage(&pStage->texture, surf->shaderRegisters);

  // set the state
  GL_State(pStage->drawStateBits);

  RB_PrepareStageTexturing(pStage, surf, ac);

  // draw it
  RB_DrawElementsWithCounters(tri);

  RB_FinishStageTexturing(pStage, surf, ac);

  if (pStage->vertexColor != SVC_IGNORE) {
    glDisableClientState(GL_COLOR_ARRAY);

    GL_SelectTexture(1);
    GL_TexEnv(GL_MODULATE);
    globalImages->BindNull();
    GL_SelectTexture(0);
    GL_TexEnv(GL_MODULATE);
  }
}

/*
====================
RB_ARB2_RenderCustomSpecialShaderStage
====================
*/
void RB_ARB2_RenderSpecialShaderStage(const float* regs, const shaderStage_t* pStage, newShaderStage_t* newStage, const srfTriangles_t	*tri) {

  idDrawVert *ac = (idDrawVert *)vertexCache.Position(tri->ambientCache);
  glVertexPointer(3, GL_FLOAT, sizeof(idDrawVert), ac->xyz.ToFloatPtr());
  glTexCoordPointer(2, GL_FLOAT, sizeof(idDrawVert), reinterpret_cast<void *>(&ac->st));
  glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(idDrawVert), (void *)&ac->color);
  glVertexAttribPointerARB(9, 3, GL_FLOAT, false, sizeof(idDrawVert), ac->tangents[0].ToFloatPtr());
  glVertexAttribPointerARB(10, 3, GL_FLOAT, false, sizeof(idDrawVert), ac->tangents[1].ToFloatPtr());
  glNormalPointer(GL_FLOAT, sizeof(idDrawVert), ac->normal.ToFloatPtr());

  glEnableClientState(GL_COLOR_ARRAY);
  glEnableVertexAttribArrayARB(9);
  glEnableVertexAttribArrayARB(10);
  glEnableClientState(GL_NORMAL_ARRAY);

  GL_State(pStage->drawStateBits);

  glBindProgramARB(GL_VERTEX_PROGRAM_ARB, newStage->vertexProgram);
  glEnable(GL_VERTEX_PROGRAM_ARB);

#if 0
  // megaTextures bind a lot of images and set a lot of parameters
  if (newStage->megaTexture) {
    newStage->megaTexture->SetMappingForSurface(tri);
    idVec3	localViewer;
    R_GlobalPointToLocal(surf->space->modelMatrix, backEnd.viewDef->renderView.vieworg, localViewer);
    newStage->megaTexture->BindForViewOrigin(localViewer);
  }
#endif

  for (int i = 0; i < newStage->numVertexParms; i++) {
    float	parm[4];
    parm[0] = regs[newStage->vertexParms[i][0]];
    parm[1] = regs[newStage->vertexParms[i][1]];
    parm[2] = regs[newStage->vertexParms[i][2]];
    parm[3] = regs[newStage->vertexParms[i][3]];
    glProgramLocalParameter4fvARB(GL_VERTEX_PROGRAM_ARB, i, parm);
  }

  for (int i = 0; i < newStage->numFragmentProgramImages; i++) {
    if (newStage->fragmentProgramImages[i]) {
      GL_SelectTexture(i);
      newStage->fragmentProgramImages[i]->Bind();
    }
  }
  glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, newStage->fragmentProgram);
  glEnable(GL_FRAGMENT_PROGRAM_ARB);

  // draw it
  RB_DrawElementsWithCounters(tri);

  for (int i = 1; i < newStage->numFragmentProgramImages; i++) {
    if (newStage->fragmentProgramImages[i]) {
      GL_SelectTexture(i);
      globalImages->BindNull();
    }
  }
  if (newStage->megaTexture) {
    newStage->megaTexture->Unbind();
  }

  GL_SelectTexture(0);

  glDisable(GL_VERTEX_PROGRAM_ARB);
  glDisable(GL_FRAGMENT_PROGRAM_ARB);
  // Fixme: Hack to get around an apparent bug in ATI drivers.  Should remove as soon as it gets fixed.
  glBindProgramARB(GL_VERTEX_PROGRAM_ARB, 0);

  glDisableClientState(GL_COLOR_ARRAY);
  glDisableVertexAttribArrayARB(9);
  glDisableVertexAttribArrayARB(10);
  glDisableClientState(GL_NORMAL_ARRAY);
}

/*
==================
RB_ARB2_DrawInteraction
==================
*/
void	RB_ARB2_DrawInteraction( const drawInteraction_t *din ) {
	// load all the vertex program parameters
	glProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, PP_LIGHT_ORIGIN, din->localLightOrigin.ToFloatPtr() );
	glProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, PP_VIEW_ORIGIN, din->localViewOrigin.ToFloatPtr() );
	glProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, PP_LIGHT_PROJECT_S, din->lightProjection[0].ToFloatPtr() );
	glProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, PP_LIGHT_PROJECT_T, din->lightProjection[1].ToFloatPtr() );
	glProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, PP_LIGHT_PROJECT_Q, din->lightProjection[2].ToFloatPtr() );
	glProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, PP_LIGHT_FALLOFF_S, din->lightProjection[3].ToFloatPtr() );
	glProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, PP_BUMP_MATRIX_S, din->bumpMatrix[0].ToFloatPtr() );
	glProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, PP_BUMP_MATRIX_T, din->bumpMatrix[1].ToFloatPtr() );
	glProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, PP_DIFFUSE_MATRIX_S, din->diffuseMatrix[0].ToFloatPtr() );
	glProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, PP_DIFFUSE_MATRIX_T, din->diffuseMatrix[1].ToFloatPtr() );
	glProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, PP_SPECULAR_MATRIX_S, din->specularMatrix[0].ToFloatPtr() );
	glProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, PP_SPECULAR_MATRIX_T, din->specularMatrix[1].ToFloatPtr() );

	static const float zero[4] = { 0, 0, 0, 0 };
	static const float one[4] = { 1, 1, 1, 1 };
	static const float negOne[4] = { -1, -1, -1, -1 };

	switch ( din->vertexColor ) {
	case SVC_IGNORE:
		glProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, PP_COLOR_MODULATE, zero );
		glProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, PP_COLOR_ADD, one );
		break;
	case SVC_MODULATE:
		glProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, PP_COLOR_MODULATE, one );
		glProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, PP_COLOR_ADD, zero );
		break;
	case SVC_INVERSE_MODULATE:
		glProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, PP_COLOR_MODULATE, negOne );
		glProgramEnvParameter4fvARB( GL_VERTEX_PROGRAM_ARB, PP_COLOR_ADD, one );
		break;
	}

	// set the constant colors
	glProgramEnvParameter4fvARB( GL_FRAGMENT_PROGRAM_ARB, 0, din->diffuseColor.ToFloatPtr() );
	glProgramEnvParameter4fvARB( GL_FRAGMENT_PROGRAM_ARB, 1, din->specularColor.ToFloatPtr() );

	// set the textures

	// texture 1 will be the per-surface bump map
	GL_SelectTextureNoClient( 1 );
	din->bumpImage->Bind();

	// texture 2 will be the light falloff texture
	GL_SelectTextureNoClient( 2 );
	din->lightFalloffImage->Bind();

	// texture 3 will be the light projection texture
	GL_SelectTextureNoClient( 3 );
	din->lightImage->Bind();

	// texture 4 is the per-surface diffuse map
	GL_SelectTextureNoClient( 4 );
	din->diffuseImage->Bind();

	// texture 5 is the per-surface specular map
	GL_SelectTextureNoClient( 5 );
	din->specularImage->Bind();

	// draw it
	RB_DrawElementsWithCounters( din->surf->geo );
}


/*
=============
RB_ARB2_CreateDrawInteractions

=============
*/
void RB_ARB2_CreateDrawInteractions( const drawSurf_t *surf ) {
	if ( !surf ) {
		return;
	}

	// perform setup here that will be constant for all interactions
	GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHMASK | backEnd.depthFunc );

	// bind the vertex program
	glBindProgramARB( GL_VERTEX_PROGRAM_ARB, VPROG_INTERACTION );
	glBindProgramARB( GL_FRAGMENT_PROGRAM_ARB, FPROG_INTERACTION );

	glEnable(GL_VERTEX_PROGRAM_ARB);
	glEnable(GL_FRAGMENT_PROGRAM_ARB);

	// enable the vertex arrays
	glEnableVertexAttribArrayARB( 8 );
	glEnableVertexAttribArrayARB( 9 );
	glEnableVertexAttribArrayARB( 10 );
	glEnableVertexAttribArrayARB( 11 );
	glEnableClientState( GL_COLOR_ARRAY );

	// texture 0 is the normalization cube map for the vector towards the light
	GL_SelectTextureNoClient( 0 );
	if ( backEnd.vLight->lightShader->IsAmbientLight() ) {
		globalImages->ambientNormalMap->Bind();
	} else {
		globalImages->normalCubeMapImage->Bind();
	}

	// texture 6 is the specular lookup table
	GL_SelectTextureNoClient( 6 );
  globalImages->specularTableImage->Bind();

	for ( ; surf ; surf=surf->nextOnLight ) {
		// perform setup here that will not change over multiple interaction passes

		// set the vertex pointers
		idDrawVert	*ac = (idDrawVert *)vertexCache.Position( surf->geo->ambientCache );
		glColorPointer( 4, GL_UNSIGNED_BYTE, sizeof( idDrawVert ), ac->color );
		glVertexAttribPointerARB( 11, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->normal.ToFloatPtr() );
		glVertexAttribPointerARB( 10, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->tangents[1].ToFloatPtr() );
		glVertexAttribPointerARB( 9, 3, GL_FLOAT, false, sizeof( idDrawVert ), ac->tangents[0].ToFloatPtr() );
		glVertexAttribPointerARB( 8, 2, GL_FLOAT, false, sizeof( idDrawVert ), ac->st.ToFloatPtr() );
		glVertexPointer( 3, GL_FLOAT, sizeof( idDrawVert ), ac->xyz.ToFloatPtr() );

		// this may cause RB_ARB2_DrawInteraction to be exacuted multiple
		// times with different colors and images if the surface or light have multiple layers
		RB_CreateSingleDrawInteractions( surf, RB_ARB2_DrawInteraction );
	}

	glDisableVertexAttribArrayARB( 8 );
	glDisableVertexAttribArrayARB( 9 );
	glDisableVertexAttribArrayARB( 10 );
	glDisableVertexAttribArrayARB( 11 );
	glDisableClientState( GL_COLOR_ARRAY );

	// disable features
	GL_SelectTextureNoClient( 6 );
	globalImages->BindNull();

	GL_SelectTextureNoClient( 5 );
	globalImages->BindNull();

	GL_SelectTextureNoClient( 4 );
	globalImages->BindNull();

	GL_SelectTextureNoClient( 3 );
	globalImages->BindNull();

	GL_SelectTextureNoClient( 2 );
	globalImages->BindNull();

	GL_SelectTextureNoClient( 1 );
	globalImages->BindNull();

	backEnd.glState.currenttmu = -1;
	GL_SelectTexture( 0 );

	glDisable(GL_VERTEX_PROGRAM_ARB);
	glDisable(GL_FRAGMENT_PROGRAM_ARB);
}

/*
==================
RB_ARB2_DrawInteractions
==================
*/
void RB_ARB2_DrawInteractions( void ) {
	viewLight_t		*vLight;
	const idMaterial	*lightShader;

	GL_SelectTexture( 0 );
	glDisableClientState( GL_TEXTURE_COORD_ARRAY );

	//
	// for each light, perform adding and shadowing
	//
	for ( vLight = backEnd.viewDef->viewLights ; vLight ; vLight = vLight->next ) {
		backEnd.vLight = vLight;

		// do fogging later
		if ( vLight->lightShader->IsFogLight() ) {
			continue;
		}
		if ( vLight->lightShader->IsBlendLight() ) {
			continue;
		}

		if ( !vLight->localInteractions && !vLight->globalInteractions
			&& !vLight->translucentInteractions ) {
			continue;
		}

		lightShader = vLight->lightShader;

		// clear the stencil buffer if needed
		if ( vLight->globalShadows || vLight->localShadows ) {
			backEnd.currentScissor = vLight->scissorRect;
			if ( r_useScissor.GetBool() ) {
				glScissor( backEnd.viewDef->viewport.x1 + backEnd.currentScissor.x1, 
					backEnd.viewDef->viewport.y1 + backEnd.currentScissor.y1,
					backEnd.currentScissor.x2 + 1 - backEnd.currentScissor.x1,
					backEnd.currentScissor.y2 + 1 - backEnd.currentScissor.y1 );
			}
			glClear( GL_STENCIL_BUFFER_BIT );
		} else {
			// no shadows, so no need to read or write the stencil buffer
			// we might in theory want to use GL_ALWAYS instead of disabling
			// completely, to satisfy the invarience rules
			glStencilFunc( GL_ALWAYS, 128, 255 );
		}

		RB_StencilShadowPass( vLight->globalShadows );
		RB_ARB2_CreateDrawInteractions( vLight->localInteractions );
		RB_StencilShadowPass( vLight->localShadows );
		RB_ARB2_CreateDrawInteractions( vLight->globalInteractions );
		glDisable( GL_VERTEX_PROGRAM_ARB );	// if there weren't any globalInteractions, it would have stayed on

		// translucent surfaces never get stencil shadowed
		if ( r_skipTranslucent.GetBool() ) {
			continue;
		}

		glStencilFunc( GL_ALWAYS, 128, 255 );

		backEnd.depthFunc = GLS_DEPTHFUNC_LESS;
		RB_ARB2_CreateDrawInteractions( vLight->translucentInteractions );

		backEnd.depthFunc = GLS_DEPTHFUNC_EQUAL;
	}

	// disable stencil shadow test
	glStencilFunc( GL_ALWAYS, 128, 255 );

	GL_SelectTexture( 0 );
	glEnableClientState( GL_TEXTURE_COORD_ARRAY );
}

//===================================================================================


typedef struct {
	GLenum			target;
	GLuint			ident;
	char			name[64];
} progDef_t;

static	const int	MAX_GLPROGS = 200;

// a single file can have both a vertex program and a fragment program
static progDef_t	progs[MAX_GLPROGS] = {
	{ GL_VERTEX_PROGRAM_ARB, VPROG_INTERACTION, "interaction.vfp" },
	{ GL_FRAGMENT_PROGRAM_ARB, FPROG_INTERACTION, "interaction.vfp" },
	{ GL_VERTEX_PROGRAM_ARB, VPROG_BUMPY_ENVIRONMENT, "bumpyEnvironment.vfp" },
	{ GL_FRAGMENT_PROGRAM_ARB, FPROG_BUMPY_ENVIRONMENT, "bumpyEnvironment.vfp" },
	{ GL_VERTEX_PROGRAM_ARB, VPROG_ENVIRONMENT, "environment.vfp" },
	{ GL_FRAGMENT_PROGRAM_ARB, FPROG_ENVIRONMENT, "environment.vfp" },
	{ GL_VERTEX_PROGRAM_ARB, VPROG_GLASSWARP, "arbVP_glasswarp.txt" },
	{ GL_FRAGMENT_PROGRAM_ARB, FPROG_GLASSWARP, "arbFP_glasswarp.txt" },
  { GL_VERTEX_PROGRAM_ARB, VPROG_STENCIL_SHADOW, "shadow.vp" },

	// additional programs can be dynamically specified in materials
};

/*
=================
R_LoadARBProgram
=================
*/
void R_LoadARBProgram( int progIndex ) {
	int		ofs;
	int		err;
	idStr	fullPath = "glprogs/";
	fullPath += progs[progIndex].name;
	char	*fileBuffer;
	char	*buffer;
	char	*start, *end;

	common->Printf( "%s", fullPath.c_str() );

	// load the program even if we don't support it, so
	// fs_copyfiles can generate cross-platform data dumps
	fileSystem->ReadFile( fullPath.c_str(), (void **)&fileBuffer, NULL );
	if ( !fileBuffer ) {
		common->Printf( ": File not found\n" );
		return;
	}

	// copy to stack memory and free
	buffer = (char *)_alloca( strlen( fileBuffer ) + 1 );
	strcpy( buffer, fileBuffer );
	fileSystem->FreeFile( fileBuffer );

	if ( !glConfig.isInitialized ) {
		return;
	}

	//
	// submit the program string at start to GL
	//
	if ( progs[progIndex].ident == 0 ) {
		// allocate a new identifier for this program
		progs[progIndex].ident = PROG_USER + progIndex;
	}

	// vertex and fragment programs can both be present in a single file, so
	// scan for the proper header to be the start point, and stamp a 0 in after the end

	if ( progs[progIndex].target == GL_VERTEX_PROGRAM_ARB ) {
		if ( !glConfig.ARBVertexProgramAvailable ) {
			common->Printf( ": GL_VERTEX_PROGRAM_ARB not available\n" );
			return;
		}
		start = strstr( (char *)buffer, "!!ARBvp" );
	}
	if ( progs[progIndex].target == GL_FRAGMENT_PROGRAM_ARB ) {
		if ( !glConfig.ARBFragmentProgramAvailable ) {
			common->Printf( ": GL_FRAGMENT_PROGRAM_ARB not available\n" );
			return;
		}
		start = strstr( (char *)buffer, "!!ARBfp" );
	}
	if ( !start ) {
		common->Printf( ": !!ARB not found\n" );
		return;
	}
	end = strstr( start, "END" );

	if ( !end ) {
		common->Printf( ": END not found\n" );
		return;
	}
	end[3] = 0;

	glBindProgramARB( progs[progIndex].target, progs[progIndex].ident );
	glGetError();

	glProgramStringARB( progs[progIndex].target, GL_PROGRAM_FORMAT_ASCII_ARB,
		strlen( start ), (unsigned char *)start );

	err = glGetError();
	glGetIntegerv( GL_PROGRAM_ERROR_POSITION_ARB, (GLint *)&ofs );
	if ( err == GL_INVALID_OPERATION ) {
		const GLubyte *str = glGetString( GL_PROGRAM_ERROR_STRING_ARB );
		common->Printf( "\nGL_PROGRAM_ERROR_STRING_ARB: %s\n", str );
		if ( ofs < 0 ) {
			common->Printf( "GL_PROGRAM_ERROR_POSITION_ARB < 0 with error\n" );
		} else if ( ofs >= (int)strlen( (char *)start ) ) {
			common->Printf( "error at end of program\n" );
		} else {
			common->Printf( "error at %i:\n%s", ofs, start + ofs );
		}
		return;
	}
	if ( ofs != -1 ) {
		common->Printf( "\nGL_PROGRAM_ERROR_POSITION_ARB != -1 without error\n" );
		return;
	}

	common->Printf( "\n" );
}

/*
==================
R_FindARBProgram

Returns a GL identifier that can be bound to the given target, parsing
a text file if it hasn't already been loaded.
==================
*/
int R_FindARBProgram( GLenum target, const char *program ) {
	int		i;
	idStr	stripped = program;

	stripped.StripFileExtension();

	// see if it is already loaded
	for ( i = 0 ; progs[i].name[0] ; i++ ) {
		if ( progs[i].target != target ) {
			continue;
		}

		idStr	compare = progs[i].name;
		compare.StripFileExtension();

		if ( !idStr::Icmp( stripped.c_str(), compare.c_str() ) ) {
			return progs[i].ident;
		}
	}

	if ( i == MAX_GLPROGS ) {
		common->Error( "R_FindARBProgram: MAX_GLPROGS" );
	}

	// add it to the list and load it
	progs[i].ident = (program_t)0;	// will be gen'd by R_LoadARBProgram
	progs[i].target = target;
	strncpy( progs[i].name, program, sizeof( progs[i].name ) - 1 );

	R_LoadARBProgram( i );

	return progs[i].ident;
}

/*
==================
R_ReloadARBPrograms_f
==================
*/
void R_ReloadARBPrograms_f( const idCmdArgs &args ) {
	int		i;

	common->Printf( "----- R_ReloadARBPrograms -----\n" );
	for ( i = 0 ; progs[i].name[0] ; i++ ) {
		R_LoadARBProgram( i );
	}
	common->Printf( "-------------------------------\n" );
}

/*
==================
R_ARB2_Init

==================
*/
void R_ARB2_Init( void ) {

	common->Printf( "---------- R_ARB2_Init ----------\n" );

	if ( !glConfig.ARBVertexProgramAvailable || !glConfig.ARBFragmentProgramAvailable ) {
		common->Printf( "Not available.\n" );
		return;
	}

	common->Printf( "Available.\n" );

	common->Printf( "---------------------------------\n" );
}

