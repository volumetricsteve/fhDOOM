#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"
#include "RenderProgram.h"
#include "RenderMatrix.h"
#include "RenderList.h"

idCVar r_smObjectCulling( "r_smObjectCulling", "1", CVAR_RENDERER | CVAR_BOOL | CVAR_ARCHIVE, "cull objects/surfaces that are outside the shadow/light frustum when rendering shadow maps" );
idCVar r_smFaceCullMode( "r_smFaceCullMode", "2", CVAR_RENDERER|CVAR_INTEGER | CVAR_ARCHIVE, "Determines which faces should be rendered to shadow map: 0=front, 1=back, 2=front-and-back");
idCVar r_smFov( "r_smFov", "93", CVAR_RENDERER|CVAR_FLOAT | CVAR_ARCHIVE, "fov used when rendering point light shadow maps");
idCVar r_smFarClip( "r_smFarClip", "-1", CVAR_RENDERER|CVAR_INTEGER | CVAR_ARCHIVE, "far clip distance for rendering shadow maps: -1=infinite-z, 0=max light radius, other=fixed distance");
idCVar r_smNearClip( "r_smNearClip", "1", CVAR_RENDERER|CVAR_INTEGER | CVAR_ARCHIVE, "near clip distance for rendering shadow maps");
idCVar r_smUseStaticOcclusion( "r_smUseStaticOcclusion", "1", CVAR_RENDERER | CVAR_BOOL | CVAR_ARCHIVE, "");
idCVar r_smSkipStaticOcclusion( "r_smSkipStaticOcclusion", "0", CVAR_RENDERER | CVAR_BOOL, "");
idCVar r_smSkipNonStaticOcclusion( "r_smSkipNonStaticOcclusion", "0", CVAR_RENDERER | CVAR_BOOL, "");
idCVar r_smSkipMovingLights( "r_smSkipMovingLights", "0", CVAR_RENDERER | CVAR_BOOL, "");

idCVar r_smLod( "r_smQuality", "-1", CVAR_RENDERER | CVAR_INTEGER | CVAR_ARCHIVE, "" );

idCVar r_smPolyOffsetFactor( "r_smPolyOffsetFactor", "8", CVAR_RENDERER | CVAR_FLOAT | CVAR_ARCHIVE, "" );
idCVar r_smPolyOffsetBias( "r_smPolyOffsetBias", "26", CVAR_RENDERER | CVAR_FLOAT | CVAR_ARCHIVE, "" );
idCVar r_smSoftness( "r_smSoftness", "1", CVAR_RENDERER | CVAR_FLOAT | CVAR_ARCHIVE, "" );
idCVar r_smBrightness( "r_smBrightness", "0.15", CVAR_RENDERER | CVAR_FLOAT | CVAR_ARCHIVE, "" );



static const int CULL_RECEIVER = 1;	// still draw occluder, but it is out of the view
static const int CULL_OCCLUDER_AND_RECEIVER = 2;	// the surface doesn't effect the view at all

static float viewLightAxialSize;
static const int firstShadowMapTextureUnit = 6;

class fhLightViewPlanes {
public:
	explicit fhLightViewPlanes() {
	}
	
	void Set(const idVec3& origin, const fhRenderMatrix& viewMatrix) {
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

		// is this normalization necessary?
		for (int i = 0; i < 6; i++) {
			globalFrustum[i].ToVec4().ToVec3().Normalize();
		}

		for (int i = 2; i < 6; i++) {
			globalFrustum[i][3] = -(origin * globalFrustum[i].ToVec4().ToVec3());
		}
	}

	bool Cull(const idRenderEntityLocal *entityDef) const {
		idPlane	localPlanes[6];		
		for (int plane = 0; plane < 6; plane++) {
			R_GlobalPlaneToLocal( entityDef->modelMatrix, globalFrustum[plane], localPlanes[plane] );
		}

		// cull the entire entity bounding box
		// has referenceBounds been tightened to the actual model bounds?
		idVec3	corners[8];
		for (int i = 0; i < 8; i++) {
			corners[i][0] = entityDef->referenceBounds[i & 1][0];
			corners[i][1] = entityDef->referenceBounds[(i >> 1) & 1][1];
			corners[i][2] = entityDef->referenceBounds[(i >> 2) & 1][2];
		}

		for (int plane = 0; plane < 6; plane++) {
			int j = 0;
			for (; j < 8; j++) {
				// if a corner is on the negative side (inside) of the frustum, the surface is not culled
				// by this plane
				if (corners[j] * localPlanes[plane].ToVec4().ToVec3() + localPlanes[plane][3] < 0) {
					break;
				}
			}
			if (j == 8) {
				return true; // all points outside the light
			}
		}

		return false;
	}

private:
	idPlane globalFrustum[6];
};

class ShadowRenderList : public fhRenderList<drawShadow_t> {
public:
	ShadowRenderList() {
		memset(&dummy, 0, sizeof(dummy));
		dummy.parms.shaderParms[0] = 1;
		dummy.parms.shaderParms[1] = 1;
		dummy.parms.shaderParms[2] = 1;
		dummy.parms.shaderParms[3] = 1;
		dummy.modelMatrix[0] = 1;
		dummy.modelMatrix[5] = 1;
		dummy.modelMatrix[10] = 1;
		dummy.modelMatrix[15] = 1;
	}


	void AddInteractions( viewLight_t* vlight, const fhRenderMatrix* viewMatrices, int numViewMatrices ) {
		assert(numViewMatrices <= 6);

		if(vlight->lightDef->lightHasMoved && r_smSkipMovingLights.GetBool()) {
			return;
		}

		const idVec3 origin = vlight->lightDef->globalLightOrigin;

		fhLightViewPlanes viewPlanes[6];
		for(int i=0; i<numViewMatrices; ++i) {
			viewPlanes[i].Set(origin, viewMatrices[i]);
		}

		bool staticOcclusionGeometryRendered = false;

		if(vlight->lightDef->parms.occlusionModel && !vlight->lightDef->lightHasMoved && r_smUseStaticOcclusion.GetBool()) {

			if(!r_smSkipStaticOcclusion.GetBool()) {
				int numSurfaces = vlight->lightDef->parms.occlusionModel->NumSurfaces();
				for( int i = 0; i < numSurfaces; ++i ) {
					auto surface = vlight->lightDef->parms.occlusionModel->Surface(i);				
					AddSurfaceInteraction(&dummy, surface->geometry, surface->shader, ~0);
				}
			}

			staticOcclusionGeometryRendered = true;
		}

		if( r_smSkipNonStaticOcclusion.GetBool() ) {
			return;
		}		
		
		for (idInteraction* inter = vlight->lightDef->firstInteraction; inter; inter = inter->lightNext) {
			const idRenderEntityLocal *entityDef = inter->entityDef;
			
			if (!entityDef) {
				continue;
			}

			if (entityDef->parms.noShadow) {
				continue;
			}

			if (inter->numSurfaces < 1) {
				continue;
			}

			unsigned visibleSides = ~0;

			if (r_smObjectCulling.GetBool() && numViewMatrices > 0) {
				visibleSides = 0;
				for(int i=0; i<numViewMatrices; ++i) {
					if(!viewPlanes[i].Cull(entityDef)) {
						visibleSides |= (1 << i);
					}
				}				
			}

			if(!visibleSides)
				continue;

			const int num = inter->numSurfaces;
			for (int i = 0; i < num; i++) {
				const auto& surface = inter->surfaces[i];
				const idMaterial* material = surface.shader;				

				if (staticOcclusionGeometryRendered && surface.isStaticWorldModel) {
					continue;
				}

				const auto* tris = surface.ambientTris;
				if (!tris || tris->numVerts < 3 || !material) {
					continue;
				}

				AddSurfaceInteraction( entityDef, tris, material, visibleSides);
			}
		}
	}

	void Submit(const float* shadowViewMatrix, const float* shadowProjectionMatrix, int side, int lod) const {
		fhRenderProgram::SetProjectionMatrix( shadowProjectionMatrix );
		fhRenderProgram::SetViewMatrix( shadowViewMatrix );
		fhRenderProgram::SetAlphaTestEnabled( false );
		fhRenderProgram::SetDiffuseMatrix( idVec4::identityS, idVec4::identityT );
		fhRenderProgram::SetAlphaTestThreshold( 0.5f );					

		const idRenderEntityLocal *currentEntity = nullptr;
		bool currentAlphaTest = false;
		bool currentHasTextureMatrix = false;

		const int sideBit = (1 << side);
		const int num = Num();

		for(int i=0; i<num; ++i) {
			const auto& drawShadow = (*this)[i];			

			if (!(drawShadow.visibleFlags & sideBit)) {
				continue;
			}

			if (!drawShadow.tris->ambientCache) {
				R_CreateAmbientCache( const_cast<srfTriangles_t *>(drawShadow.tris), false );
			}

			const auto offset = vertexCache.Bind( drawShadow.tris->ambientCache );
			GL_SetupVertexAttributes( fhVertexLayout::DrawPosTexOnly, offset );

			if(currentEntity != drawShadow.entity) {
				fhRenderProgram::SetModelMatrix(drawShadow.entity->modelMatrix);
				currentEntity = drawShadow.entity;
			}

			if(drawShadow.texture) {
				if(!currentAlphaTest) {
					fhRenderProgram::SetAlphaTestEnabled( true );					
					currentAlphaTest = true;
				}
				
				drawShadow.texture->Bind( 0 );

				if(drawShadow.hasTextureMatrix) {
					fhRenderProgram::SetDiffuseMatrix( drawShadow.textureMatrix[0], drawShadow.textureMatrix[1] );
					currentHasTextureMatrix = true;
				}
				else if(currentHasTextureMatrix) {
					fhRenderProgram::SetDiffuseMatrix( idVec4::identityS, idVec4::identityT );
					currentHasTextureMatrix = false;
				}
			}
			else if(currentAlphaTest) {
				fhRenderProgram::SetAlphaTestEnabled( false );
				currentAlphaTest = false;
			}

			RB_DrawElementsWithCounters( drawShadow.tris );
			backEnd.stats.drawcalls[backEndGroup::ShadowMap0 + lod] += 1;
		}
	}

private:

	void AddSurfaceInteraction(const idRenderEntityLocal *entityDef, const srfTriangles_t *tri, const idMaterial* material, unsigned visibleSides) {

		if (!material->SurfaceCastsSoftShadow()) {
			return;
		}

		drawShadow_t drawShadow;
		drawShadow.tris = tri;
		drawShadow.entity = entityDef;
		drawShadow.texture = nullptr;	
		drawShadow.visibleFlags = visibleSides;

		// we may have multiple alpha tested stages
		if (material->Coverage() == MC_PERFORATED) {
			// if the only alpha tested stages are condition register omitted,
			// draw a normal opaque surface

			float *regs = (float *)R_ClearedFrameAlloc( material->GetNumRegisters() * sizeof(float) );
			material->EvaluateRegisters( regs, entityDef->parms.shaderParms, backEnd.viewDef, nullptr );

			// perforated surfaces may have multiple alpha tested stages
			for (int stage = 0; stage < material->GetNumStages(); stage++) {
				const shaderStage_t* pStage = material->GetStage( stage );

				if (!pStage->hasAlphaTest) {
					continue;
				}

				if (regs[pStage->conditionRegister] == 0) {
					continue;
				}

				drawShadow.texture = pStage->texture.image;
				drawShadow.alphaTestThreshold = 0.5f;

				if (pStage->texture.hasMatrix) {
					drawShadow.hasTextureMatrix = true;
					RB_GetShaderTextureMatrix( regs, &pStage->texture, drawShadow.textureMatrix );
				}
				else {
					drawShadow.hasTextureMatrix = false;
				}

				break;
			}
		}

		Append(drawShadow);
	}

	idRenderEntityLocal dummy;
};

static fhRenderMatrix RB_CreateShadowViewMatrix(const viewLight_t* vLight, int side) {
	fhRenderMatrix viewMatrix;

	memset( &viewMatrix, 0, sizeof(viewMatrix) );

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

	return viewMatrix;
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


static void RB_RenderShadowCasters(const viewLight_t *vLight, const float* shadowViewMatrix, const float* shadowProjectionMatrix, int lod) {	
	
	fhRenderProgram::SetProjectionMatrix( shadowProjectionMatrix );
	fhRenderProgram::SetViewMatrix( shadowViewMatrix );		
	fhRenderProgram::SetAlphaTestEnabled(false);
	fhRenderProgram::SetDiffuseMatrix(idVec4::identityS, idVec4::identityT);

	const idRenderEntityLocal *currentEntity = nullptr;
	bool currentAlphaTest = false;
	bool currentHasDiffuseMatrix = false;
	
	for (idInteraction *inter = vLight->lightDef->firstInteraction; inter; inter = inter->lightNext) {
		const idRenderEntityLocal *entityDef = inter->entityDef;

		if (!entityDef) {
			continue;
		}
		if (entityDef->parms.noShadow) {
			continue;
		}
		if (inter->numSurfaces < 1) {
			continue;
		}		

		// draw each surface
		for (int i = 0; i < inter->numSurfaces; i++) {
			surfaceInteraction_t	*surfInt = &inter->surfaces[i];
			const idMaterial* shader = surfInt->shader;

			if (!surfInt->ambientTris || !surfInt->shader) {
				continue;
			}			

			if (!shader->SurfaceCastsSoftShadow()) {
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
			GL_SetupVertexAttributes(fhVertexLayout::DrawPosTexOnly, offset);			

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
					pStage->texture.image->Bind(0);

					fhRenderProgram::SetAlphaTestEnabled(true);
					fhRenderProgram::SetAlphaTestThreshold(0.5f);
					currentAlphaTest = true;

					
					if (pStage->texture.hasMatrix) {						
						idVec4 textureMatrix[2] = {idVec4(1,0,0,0), idVec4(0,1,0,0)};						
						RB_GetShaderTextureMatrix(regs, &pStage->texture, textureMatrix);
						fhRenderProgram::SetDiffuseMatrix(textureMatrix[0], textureMatrix[1]);
						currentHasDiffuseMatrix = true;
					}
					else if (currentHasDiffuseMatrix) {
						fhRenderProgram::SetDiffuseMatrix( idVec4::identityS, idVec4::identityT );
						currentHasDiffuseMatrix = false;
					}

					if(currentEntity != entityDef) {
						fhRenderProgram::SetModelMatrix(entityDef->modelMatrix);		
						currentEntity = entityDef;
					}

					// draw it
					RB_DrawElementsWithCounters(tri);
					backEnd.stats.drawcalls[backEndGroup::ShadowMap0 + lod] += 1;
					didDraw = true;
					break;
				}
			}
			
			if(!didDraw) {
				if (currentAlphaTest) {
					fhRenderProgram::SetAlphaTestEnabled( false );
					currentAlphaTest = false;
				}			

				if (currentEntity != entityDef) {
					fhRenderProgram::SetModelMatrix( entityDef->modelMatrix );
					currentEntity = entityDef;
				}

				// draw it
				backEnd.stats.drawcalls[backEndGroup::ShadowMap0 + lod] += 1;
				RB_DrawElementsWithCounters(tri);
			}
		}
	}
}

static void RB_CreateProjectedProjectionMatrix(const viewLight_t* vLight, float* m)
{
	auto parms = vLight->lightDef->parms;

	if (parms.start.LengthSqr() < 0.0001f)
		parms.start = parms.target.Normalized() * 8.0f;

	if (parms.end.LengthSqr() < 0.0001f)
		parms.end = parms.target;

	idPlane planes[4];
	R_SetLightProject( planes, idVec3(0,0,0), parms.target, parms.right, parms.up, parms.start, parms.end );

	idPlane frustrum[6];
	R_SetLightFrustum( planes, frustrum );

	idVec3 start[4];
	idVec3 dir[4];
	frustrum[0].PlaneIntersection( frustrum[1], start[0], dir[0] ); //ok
	frustrum[1].PlaneIntersection( frustrum[2], start[1], dir[1] );
	frustrum[2].PlaneIntersection( frustrum[3], start[2], dir[2] );
	frustrum[3].PlaneIntersection( frustrum[0], start[3], dir[3] );

	float scale[8];
	frustrum[4].RayIntersection( start[0], dir[0], scale[0] );
	frustrum[4].RayIntersection( start[1], dir[1], scale[1] );
	frustrum[4].RayIntersection( start[2], dir[2], scale[2] );
	frustrum[4].RayIntersection( start[3], dir[3], scale[3] );

	frustrum[5].RayIntersection( start[0], dir[0], scale[4] );
	frustrum[5].RayIntersection( start[1], dir[1], scale[5] );
	frustrum[5].RayIntersection( start[2], dir[2], scale[6] );
	frustrum[5].RayIntersection( start[3], dir[3], scale[7] );

	float n = parms.start.Length();
	float f = parms.end.Length();

	idVec3 nearPoint[4];
	nearPoint[0] = dir[0] * scale[0];
	nearPoint[1] = dir[1] * scale[1];
	nearPoint[2] = dir[2] * scale[2];
	nearPoint[3] = dir[3] * scale[3];

	float l = -((dir[0] * scale[0]) - parms.start).y;
	float r = -l;
	float b = -((dir[0] * scale[0]) - parms.start).x;
	float t = -b;
	
	memset( m, 0, sizeof(m[0]) * 16 );
	m[0] = (2.0f * n) / (r - l);
	m[5] = (2.0f * n) / (t - b);
	m[8] = (r + l) / (r - l);
	m[9] = (t + b) / (t - b);
	m[10] = -((f + n) / (f - n));
	m[11] = -1.0f;
	m[14] = -(2 * f  * n) / (f - n);
}


static void RB_RenderProjectedShadowBuffer(viewLight_t* vLight, int lod) {

	//TODO(johl): get the math straight. this is just terrible and could be done simpler and more efficient
	idVec3 origin = vLight->lightDef->parms.origin;
	idVec3 localTarget = vLight->lightDef->parms.target;
	idVec3 rotatedLocalTarget = vLight->lightDef->parms.axis * vLight->lightDef->parms.target;
	idVec3 worldTarget = vLight->lightDef->parms.origin + rotatedLocalTarget;

	idVec3 flippedOrigin = fhRenderMatrix::FlipMatrix() * origin;
	idVec3 flippedTarget = fhRenderMatrix::FlipMatrix() * worldTarget;
	idVec3 flippedUp = fhRenderMatrix::FlipMatrix() * (vLight->lightDef->parms.axis * vLight->lightDef->parms.up);
	auto viewMatrix = fhRenderMatrix::CreateLookAtMatrix(flippedOrigin, flippedTarget, flippedUp) * fhRenderMatrix::FlipMatrix();
	fhRenderMatrix projectionMatrix;	
	RB_CreateProjectedProjectionMatrix( vLight, projectionMatrix.ToFloatPtr() );

	fhFramebuffer* framebuffer = globalImages->shadowmapFramebuffer;
	framebuffer->Bind();
	glViewport( 0, 0, framebuffer->GetWidth(), framebuffer->GetHeight() );
	glScissor( 0, 0, framebuffer->GetWidth(), framebuffer->GetHeight() );

	glStencilFunc( GL_ALWAYS, 0, 255 );

	GL_State( GLS_DEPTHFUNC_LESS | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );	// make sure depth mask is off before clear
	glDepthMask( GL_TRUE );
	glEnable( GL_DEPTH_TEST );

	glClearDepth( 1.0 );
	glClear( GL_DEPTH_BUFFER_BIT );

	backEnd.currentSpace = NULL;	
	auto viewProjection = projectionMatrix * viewMatrix;

	memcpy(&backEnd.shadowViewProjection[0][0], viewProjection.ToFloatPtr(), sizeof(float)*16);
	memcpy(backEnd.testProjectionMatrix, viewProjection.ToFloatPtr(), sizeof(float)*16);
	memcpy(backEnd.testViewMatrix, viewMatrix.ToFloatPtr(), sizeof(float)*16);

	RB_RenderShadowCasters( vLight, viewMatrix.ToFloatPtr(), projectionMatrix.ToFloatPtr(), lod );

	backEnd.stats.passes[backEndGroup::ShadowMap0 + lod] += 1;
}

//offsets to pack 6 smaller textures into a texture atlas (3x2)
static const idVec2 sideOffsets[] = {
	idVec2(0,       0),
	idVec2(1.0/3.0, 0),
	idVec2(1.0/1.5, 0),
	idVec2(0,       0.5),
	idVec2(1.0/3.0, 0.5),
	idVec2(1.0/1.5, 0.5)
};

static void RB_RenderPointLightShadowBuffer2( const ShadowRenderList& renderlist, const fhRenderMatrix& projectionMatrix, const fhRenderMatrix& viewMatrix, int side, int lod ) {
	fhFramebuffer* framebuffer = globalImages->shadowmapFramebuffer;	

	const idVec2 scale = idVec2(1.0/3.0, 1.0/2.0) / (1 << lod);
	const idVec2 offset = sideOffsets[side];
	const int width = framebuffer->GetWidth() * scale.x;
	const int height = framebuffer->GetHeight() * scale.y;
	const int offsetX = framebuffer->GetWidth() * offset.x;
	const int offsetY = framebuffer->GetHeight() * offset.y;

	backEnd.shadowCoords[side].scale = scale;
	backEnd.shadowCoords[side].offset = offset;

	glViewport( offsetX, offsetY, width, height);
	glScissor( offsetX, offsetY, width, height);

	*reinterpret_cast<fhRenderMatrix*>(&backEnd.shadowViewProjection[side][0]) = projectionMatrix * viewMatrix;

	renderlist.Submit(viewMatrix.ToFloatPtr(), projectionMatrix.ToFloatPtr(), side, lod);
	backEnd.stats.passes[backEndGroup::ShadowMap0 + lod] += 1;
}

void RB_RenderShadowMaps(viewLight_t* vLight) {

	const idMaterial* lightShader = vLight->lightShader;
	
	if (lightShader->IsFogLight() || lightShader->IsBlendLight()) {
		return;
	}

	if (!vLight->localInteractions && !vLight->globalInteractions
		&& !vLight->translucentInteractions) {
		return;
	}

	if(!vLight->lightShader->LightCastsShadows()) {
		return;
	}

	int lod = vLight->shadowMapLod;
	if(r_smLod.GetInteger() >= 0) {
		lod = r_smLod.GetInteger();
	}		

	const uint64 startTime = Sys_Microseconds();

	// all light side projections must currently match, so non-centered
	// and non-cubic lights must take the largest length
	viewLightAxialSize = R_EXP_CalcLightAxialSize(vLight);

	GL_UseProgram(shadowmapProgram);

	const float polygonOffsetBias = vLight->lightDef->ShadowPolygonOffsetBias();
	const float polygonOffsetFactor = vLight->lightDef->ShadowPolygonOffsetFactor();
	glEnable( GL_POLYGON_OFFSET_FILL );
	glPolygonOffset( polygonOffsetFactor, polygonOffsetBias );

	switch (r_smFaceCullMode.GetInteger())
	{
	case 0:
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CCW);
		break;
	case 1:
		glEnable(GL_CULL_FACE);
		glFrontFace(GL_CW);
		break;
	case 2:
	default:
		glDisable(GL_CULL_FACE);
		break;
	}	

	if(vLight->lightDef->parms.pointLight) {
		fhRenderMatrix projectionMatrix;
		if (r_smFarClip.GetInteger() < 0) {
			projectionMatrix = fhRenderMatrix::CreateInfiniteProjectionMatrix( r_smFov.GetFloat(), 1, r_smNearClip.GetFloat() );
		}
		else if (r_smFarClip.GetInteger() == 0) {
			projectionMatrix = fhRenderMatrix::CreateProjectionMatrix( r_smFov.GetFloat(), 1, r_smNearClip.GetFloat(), vLight->lightDef->GetMaximumCenterToEdgeDistance() );
		}
		else {
			projectionMatrix = fhRenderMatrix::CreateProjectionMatrix( r_smFov.GetFloat(), 1, r_smNearClip.GetFloat(), r_smFarClip.GetFloat() );
		}

		fhRenderMatrix viewMatrices[6];
		for(int side = 0; side < 6; ++side) {
			viewMatrices[side] = RB_CreateShadowViewMatrix( vLight, side );
		}

		glStencilFunc( GL_ALWAYS, 0, 255 );
		GL_State( GLS_DEPTHFUNC_LESS | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );	// make sure depth mask is off before clear
		glDepthMask( GL_TRUE );
		glEnable( GL_DEPTH_TEST );

		ShadowRenderList renderlist;
		renderlist.AddInteractions( vLight, viewMatrices, 6 );

		globalImages->BindNull( firstShadowMapTextureUnit );

		globalImages->shadowmapFramebuffer->Bind();

		glViewport(0,0,globalImages->shadowmapFramebuffer->GetWidth(),globalImages->shadowmapFramebuffer->GetHeight());
		glScissor(0,0,globalImages->shadowmapFramebuffer->GetWidth(),globalImages->shadowmapFramebuffer->GetHeight());

		glClearDepth( 1.0 );
		glClear( GL_DEPTH_BUFFER_BIT );

		const int indices = backEnd.pc.c_drawIndexes;

		for (int side = 0; side < 6; side++) {
			viewMatrices[side] = fhRenderMatrix::FlipMatrix() * viewMatrices[side];
			RB_RenderPointLightShadowBuffer2( renderlist, projectionMatrix, viewMatrices[side], side, lod );
		}

		globalImages->defaultFramebuffer->Bind();

		globalImages->shadowmapImage->Bind( firstShadowMapTextureUnit );

		if(r_ignore.GetBool()) {
			common->Printf("shadow map tris (%f %f %f): %d\n", vLight->lightDef->parms.origin.x, vLight->lightDef->parms.origin.y, vLight->lightDef->parms.origin.z, (backEnd.pc.c_drawIndexes - indices)/3 );
		}
	} else if (vLight->lightDef->parms.parallel) {		
		assert(false && "parallel shadow maps not implemented");
	}
	else {	
		globalImages->BindNull( firstShadowMapTextureUnit );
		RB_RenderProjectedShadowBuffer( vLight, lod );		
		globalImages->shadowmapImage->Bind( firstShadowMapTextureUnit );
	}

	glPolygonOffset( 0, 0 );
	glDisable( GL_POLYGON_OFFSET_FILL );
	
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW);
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

	const uint64 endTime = Sys_Microseconds();
	backEnd.stats.time[backEndGroup::ShadowMap0 + lod] += static_cast<uint32>(endTime - startTime);
}
