#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"

idCVar r_useShadowMapCulling( "r_useShadowMapCulling", "1", CVAR_RENDERER | CVAR_BOOL, "use culling when rendering shadow maps" );
idCVar r_useShadowMapMinSize( "r_useShadowMapMinSize", "128", CVAR_RENDERER | CVAR_FLOAT, "minimum size for light to have shadow maps enabled" );


static const int CULL_RECEIVER = 1;	// still draw occluder, but it is out of the view
static const int CULL_OCCLUDER_AND_RECEIVER = 2;	// the surface doesn't effect the view at all

static float viewLightAxialSize;

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

static float	s_flipMatrix[16] = {
	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};

template<typename T>
static const void* attributeOffset(T offset, const void* attributeOffset)
{
	return reinterpret_cast<const void*>((std::ptrdiff_t)offset + (std::ptrdiff_t)attributeOffset);
}


/*
==================
R_EXP_CalcLightAxialSize

all light side projections must currently match, so non-centered
and non-cubic lights must take the largest length
==================
*/
float	R_EXP_CalcLightAxialSize(const viewLight_t *vLight) {
	float	max = 0;

	if (!vLight->lightDef->parms.pointLight) {
		idVec3	dir = vLight->lightDef->parms.target - vLight->lightDef->parms.origin;
		max = dir.Length();
		return max;
	}

	for (int i = 0; i < 3; i++) {
		float	dist = fabs(vLight->lightDef->parms.lightCenter[i]);
		dist += vLight->lightDef->parms.lightRadius[i];
		if (dist > max) {
			max = dist;
		}
	}
	return max;
}

/*
==================
RB_EXP_CullInteractions

Sets surfaceInteraction_t->cullBits
==================
*/
void RB_EXP_CullInteractions(viewLight_t *vLight, idPlane frustumPlanes[6]) {
	
	for (idInteraction *inter = vLight->lightDef->firstInteraction; inter; inter = inter->lightNext) {
		const idRenderEntityLocal *entityDef = inter->entityDef;

		if (!entityDef) {
			continue;
		}
		if (inter->numSurfaces < 1) {
			continue;
		}

		int	culled = 0;

		if (r_useShadowMapCulling.GetBool()) {

			// transform light frustum into object space, positive side points outside the light
			idPlane	localPlanes[6];
			int		plane;
			for (plane = 0; plane < 6; plane++) {
				R_GlobalPlaneToLocal(entityDef->modelMatrix, frustumPlanes[plane], localPlanes[plane]);
			}

			// cull the entire entity bounding box
			// has referenceBounds been tightened to the actual model bounds?
			idVec3	corners[8];
			for (int i = 0; i < 8; i++) {
				corners[i][0] = entityDef->referenceBounds[i & 1][0];
				corners[i][1] = entityDef->referenceBounds[(i >> 1) & 1][1];
				corners[i][2] = entityDef->referenceBounds[(i >> 2) & 1][2];
			}

			for (plane = 0; plane < 6; plane++) {
				int		j;
				for (j = 0; j < 8; j++) {
					// if a corner is on the negative side (inside) of the frustum, the surface is not culled
					// by this plane
					if (corners[j] * localPlanes[plane].ToVec4().ToVec3() + localPlanes[plane][3] < 0) {
						break;
					}
				}
				if (j == 8) {
					break;			// all points outside the light
				}
			}
			if (plane < 6) {
				culled = CULL_OCCLUDER_AND_RECEIVER;
			}
		}

		for (int i = 0; i < inter->numSurfaces; i++) {
			surfaceInteraction_t	*surfInt = &inter->surfaces[i];

			

			if (!surfInt->ambientTris) {
				continue;
			}
			surfInt->expCulled = culled;
		}

	}
}

static void RB_RenderShadowCasters(const viewLight_t *vLight, const float* shadowViewMatrix) {
	for (idInteraction *inter = vLight->lightDef->firstInteraction; inter; inter = inter->lightNext) {
		const idRenderEntityLocal *entityDef = inter->entityDef;
		if (!entityDef) {
			continue;
		}
		if (inter->numSurfaces < 1) {
			continue;
		}

		// no need to check for current on this, because each interaction is always
		// a different space
		float	matrix[16];
		myGlMultMatrix(inter->entityDef->modelMatrix, shadowViewMatrix, matrix);

		// draw each surface
		for (int i = 0; i < inter->numSurfaces; i++) {
			surfaceInteraction_t	*surfInt = &inter->surfaces[i];

			if (!surfInt->ambientTris) {
				continue;
			}

			const idMaterial* shader = surfInt->shader;

			if (shader && !shader->SurfaceCastsShadow() && shader->Coverage() != MC_PERFORATED) {
				continue;
			}

			// cull it
			if (surfInt->expCulled == CULL_OCCLUDER_AND_RECEIVER) {
				continue;
			}

			// render it
			const srfTriangles_t *tri = surfInt->ambientTris;
			if(!tri || !tri->numVerts) {
				continue;
			}

			if (!tri->ambientCache) {
				R_CreateAmbientCache(const_cast<srfTriangles_t *>(tri), false);
			}

			const auto offset = vertexCache.Bind(tri->ambientCache);
			glVertexAttribPointer(glslProgramDef_t::vertex_attrib_position, 3, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::xyzOffset));
			glVertexAttribPointer(glslProgramDef_t::vertex_attrib_texcoord, 2, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::texcoordOffset));
			glUniformMatrix4fv(glslProgramDef_t::uniform_modelViewMatrix, 1, false, matrix);
			glUniformMatrix4fv(glslProgramDef_t::uniform_projectionMatrix, 1, false, GL_ProjectionMatrix.Top());			


			bool didDraw = false;
			
			// we may have multiple alpha tested stages
			if (shader->Coverage() == MC_PERFORATED) {
				// if the only alpha tested stages are condition register omitted,
				// draw a normal opaque surface
				
				float *regs = (float *)R_ClearedFrameAlloc(shader->GetNumRegisters() * sizeof(float));
				shader->EvaluateRegisters(regs, entityDef->parms.shaderParms, backEnd.viewDef, nullptr);


				// perforated surfaces may have multiple alpha tested stages
				for (int stage = 0; stage < shader->GetNumStages(); stage++) {
					const shaderStage_t* pStage = shader->GetStage(stage);

					if (!pStage->hasAlphaTest) {
						continue;
					}

					if (regs[pStage->conditionRegister] == 0) {
						continue;
					}

					// bind the texture
					pStage->texture.image->Bind();

					glUniform1i(glslProgramDef_t::uniform_alphaTestEnabled, 1);
					glUniform1f(glslProgramDef_t::uniform_alphaTestThreshold, 0.5f);

					idVec4 textureMatrix[2] = {idVec4(1,0,0,0), idVec4(0,1,0,0)};
					if (pStage->texture.hasMatrix) {						
						RB_GetShaderTextureMatrix(regs, &pStage->texture, textureMatrix);
					}
					glUniform4fv(glslProgramDef_t::uniform_diffuseMatrixS, 1, textureMatrix[0].ToFloatPtr());
					glUniform4fv(glslProgramDef_t::uniform_diffuseMatrixT, 1, textureMatrix[1].ToFloatPtr());

					// draw it
					RB_DrawElementsWithCounters(tri);
					backEnd.pc.c_shadowMapDraws++;
					didDraw = true;
					break;
				}
			}
			
			if(!didDraw) {
				glUniform1i(glslProgramDef_t::uniform_alphaTestEnabled, 0);
				// draw it
				RB_DrawElementsWithCounters(tri);
				backEnd.pc.c_shadowMapDraws++;
			}
		}
	}
}

static void RB_RenderShadowBuffer(viewLight_t* vLight, int side) {


	//
	// set up 90 degree projection matrix
	//
	const float	fov = 90;
	const float zNear = 4;

	const float ymax = zNear * tan(fov * idMath::PI / 360.0f);
	const float ymin = -ymax;

	const float xmax = zNear * tan(fov * idMath::PI / 360.0f);
	const float xmin = -xmax;

	const float width = xmax - xmin;
	const float height = ymax - ymin;

	float lightProjectionMatrix[16];
	lightProjectionMatrix[0] = 2 * zNear / width;
	lightProjectionMatrix[4] = 0;
	lightProjectionMatrix[8] = 0;
	lightProjectionMatrix[12] = 0;

	lightProjectionMatrix[1] = 0;
	lightProjectionMatrix[5] = 2 * zNear / height;
	lightProjectionMatrix[9] = 0;
	lightProjectionMatrix[13] = 0;

	// this is the far-plane-at-infinity formulation, and
	// crunches the Z range slightly so w=0 vertexes do not
	// rasterize right at the wraparound point
	lightProjectionMatrix[2] = 0;
	lightProjectionMatrix[6] = 0;
	lightProjectionMatrix[10] = -0.999f;
	lightProjectionMatrix[14] = -2.0f * zNear;

	lightProjectionMatrix[3] = 0;
	lightProjectionMatrix[7] = 0;
	lightProjectionMatrix[11] = -1;
	lightProjectionMatrix[15] = 0;

	fhFramebuffer* framebuffer = globalImages->shadowmapFramebuffer[side];
	framebuffer->Bind();

	GL_ProjectionMatrix.Push();
	GL_ProjectionMatrix.Load(lightProjectionMatrix);

	glViewport(0, 0, framebuffer->width, framebuffer->height);
	glScissor(0, 0, framebuffer->width, framebuffer->height);

	glStencilFunc(GL_ALWAYS, 0, 255);

	GL_State(GLS_DEPTHFUNC_LESS | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO);	// make sure depth mask is off before clear
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);

	//TODO(johl): we don't need to clear to color buffer
	glClearColor(0,1,0,1);
	glClearDepth(1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// draw all the occluders	

	backEnd.currentSpace = NULL;



	float	viewMatrix[16];
	

	if (side == -1) {
		// projected light
		idVec3 vec = vLight->lightDef->parms.target;
		vec.Normalize();
		viewMatrix[0] = vec[0];
		viewMatrix[4] = vec[1];
		viewMatrix[8] = vec[2];

		vec = vLight->lightDef->parms.right;
		vec.Normalize();
		viewMatrix[1] = -vec[0];
		viewMatrix[5] = -vec[1];
		viewMatrix[9] = -vec[2];

		vec = vLight->lightDef->parms.up;
		vec.Normalize();
		viewMatrix[2] = vec[0];
		viewMatrix[6] = vec[1];
		viewMatrix[10] = vec[2];
	}
	else {
		// side of a point light
		memset(viewMatrix, 0, sizeof(viewMatrix));
		switch (side) {
		case 0:
			viewMatrix[0] = 1;
			viewMatrix[9] = 1;
			viewMatrix[6] = -1;
			break;
		case 1:
			viewMatrix[0] = -1;
			viewMatrix[9] = -1;
			viewMatrix[6] = -1;
			break;
		case 2:
			viewMatrix[4] = 1;
			viewMatrix[1] = -1;
			viewMatrix[10] = 1;
			break;
		case 3:
			viewMatrix[4] = -1;
			viewMatrix[1] = -1;
			viewMatrix[10] = -1;
			break;
		case 4:
			viewMatrix[8] = 1;
			viewMatrix[1] = -1;
			viewMatrix[6] = -1;
			break;
		case 5:
			viewMatrix[8] = -1;
			viewMatrix[1] = 1;
			viewMatrix[6] = -1;
			break;
		}
	}

	idVec3	origin = vLight->lightDef->globalLightOrigin;
	viewMatrix[12] = -origin[0] * viewMatrix[0] + -origin[1] * viewMatrix[4] + -origin[2] * viewMatrix[8];
	viewMatrix[13] = -origin[0] * viewMatrix[1] + -origin[1] * viewMatrix[5] + -origin[2] * viewMatrix[9];
	viewMatrix[14] = -origin[0] * viewMatrix[2] + -origin[1] * viewMatrix[6] + -origin[2] * viewMatrix[10];

	viewMatrix[3] = 0;
	viewMatrix[7] = 0;
	viewMatrix[11] = 0;
	viewMatrix[15] = 1;

	float flippedViewMatrix[16];
	
	myGlMultMatrix(viewMatrix, s_flipMatrix, flippedViewMatrix);

	if(r_useShadowMapCulling.GetBool()) {
		// create frustum planes
		idPlane	globalFrustum[6];

		// near clip
		globalFrustum[0][0] = -viewMatrix[0];
		globalFrustum[0][1] = -viewMatrix[4];
		globalFrustum[0][2] = -viewMatrix[8];
		globalFrustum[0][3] = -(origin[0] * globalFrustum[0][0] + origin[1] * globalFrustum[0][1] + origin[2] * globalFrustum[0][2]);

		// far clip
		globalFrustum[1][0] = viewMatrix[0];
		globalFrustum[1][1] = viewMatrix[4];
		globalFrustum[1][2] = viewMatrix[8];
		globalFrustum[1][3] = -globalFrustum[0][3] - viewLightAxialSize;

		// side clips
		globalFrustum[2][0] = -viewMatrix[0] + viewMatrix[1];
		globalFrustum[2][1] = -viewMatrix[4] + viewMatrix[5];
		globalFrustum[2][2] = -viewMatrix[8] + viewMatrix[9];

		globalFrustum[3][0] = -viewMatrix[0] - viewMatrix[1];
		globalFrustum[3][1] = -viewMatrix[4] - viewMatrix[5];
		globalFrustum[3][2] = -viewMatrix[8] - viewMatrix[9];

		globalFrustum[4][0] = -viewMatrix[0] + viewMatrix[2];
		globalFrustum[4][1] = -viewMatrix[4] + viewMatrix[6];
		globalFrustum[4][2] = -viewMatrix[8] + viewMatrix[10];

		globalFrustum[5][0] = -viewMatrix[0] - viewMatrix[2];
		globalFrustum[5][1] = -viewMatrix[4] - viewMatrix[6];
		globalFrustum[5][2] = -viewMatrix[8] - viewMatrix[10];

		// is this nromalization necessary?
		for (int i = 0; i < 6; i++) {
			globalFrustum[i].ToVec4().ToVec3().Normalize();
		}

		for (int i = 2; i < 6; i++) {
			globalFrustum[i][3] = -(origin * globalFrustum[i].ToVec4().ToVec3());
		}

		RB_EXP_CullInteractions(vLight, globalFrustum);
	}



	myGlMultMatrix(flippedViewMatrix, lightProjectionMatrix, &backEnd.shadowViewProjection[side][0]);
	RB_RenderShadowCasters(vLight, flippedViewMatrix);


	GL_ProjectionMatrix.Pop();

	backEnd.pc.c_shadowPasses++;
}


void RB_RenderShadowMaps(viewLight_t* vLight) {

	const idMaterial	*lightShader = vLight->lightShader;

	// do fogging later
	if (lightShader->IsFogLight()) {
		return;
	}
	if (lightShader->IsBlendLight()) {
		return;
	}

	if (!vLight->localInteractions && !vLight->globalInteractions
		&& !vLight->translucentInteractions) {
		return;
	}

	if(!vLight->lightShader->LightCastsShadows()) {
		return;
	}

	if(vLight->frustumTris->bounds.GetRadius() < r_useShadowMapMinSize.GetFloat()) {
		return;
	}

	if (!vLight->frustumTris->ambientCache) {
		R_CreateAmbientCache(const_cast<srfTriangles_t *>(vLight->frustumTris), false);
	}

	// all light side projections must currently match, so non-centered
	// and non-cubic lights must take the largest length
	viewLightAxialSize = R_EXP_CalcLightAxialSize(vLight);

	GL_UseProgram(depthProgram);
	glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
	glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);

	for (int side=0; side < 6; side++) {
		// FIXME: check for frustums completely off the screen

		// render a shadow buffer
		RB_RenderShadowBuffer(vLight, side);		
	}

	glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
	glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);
	GL_UseProgram(nullptr);

	globalImages->defaultFramebuffer->Bind();
	backEnd.currentSpace = NULL;

	//reset viewport 
	glViewport(tr.viewportOffset[0] + backEnd.viewDef->viewport.x1,
		tr.viewportOffset[1] + backEnd.viewDef->viewport.y1,
		backEnd.viewDef->viewport.x2 + 1 - backEnd.viewDef->viewport.x1,
		backEnd.viewDef->viewport.y2 + 1 - backEnd.viewDef->viewport.y1);

	// the scissor may be smaller than the viewport for subviews
	glScissor(tr.viewportOffset[0] + backEnd.viewDef->viewport.x1 + backEnd.viewDef->scissor.x1,
		tr.viewportOffset[1] + backEnd.viewDef->viewport.y1 + backEnd.viewDef->scissor.y1,
		backEnd.viewDef->scissor.x2 + 1 - backEnd.viewDef->scissor.x1,
		backEnd.viewDef->scissor.y2 + 1 - backEnd.viewDef->scissor.y1);
		backEnd.currentScissor = backEnd.viewDef->scissor;


	// texture 5 is the per-surface specular map
	GL_SelectTextureNoClient(6);
	globalImages->shadowmapDepthImage[0]->Bind();

	GL_SelectTextureNoClient(7);
	globalImages->shadowmapDepthImage[1]->Bind();

	GL_SelectTextureNoClient(8);
	globalImages->shadowmapDepthImage[2]->Bind();

	GL_SelectTextureNoClient(9);
	globalImages->shadowmapDepthImage[3]->Bind();

	GL_SelectTextureNoClient(10);
	globalImages->shadowmapDepthImage[4]->Bind();

	GL_SelectTextureNoClient(11);
	globalImages->shadowmapDepthImage[5]->Bind();
}