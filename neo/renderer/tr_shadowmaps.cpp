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

static float viewLightAxialSize;
static const int firstShadowMapTextureUnit = 6;


/*
==================
R_EXP_CalcLightAxialSize

all light side projections must currently match, so non-centered
and non-cubic lights must take the largest length
==================
*/
static float R_EXP_CalcLightAxialSize( const idRenderLightLocal *lightDef ) {
	float	max = 0;

	if (!lightDef->parms.pointLight) {
		idVec3	dir = lightDef->parms.target - lightDef->parms.origin;
		max = dir.Length();
		return max;
	}

	for (int i = 0; i < 3; i++) {
		float	dist = fabs( lightDef->parms.lightCenter[i] );
		dist += lightDef->parms.lightRadius[i];
		if (dist > max) {
			max = dist;
		}
	}
	return max;
}

/*
===================
R_Cull

cull 8 corners against given shadow frustum.
Return true of all 8 corners are outside the frustum.
===================
*/
static bool R_Cull(const idVec3* corners, const shadowFrustum_t& frustum) {

	const int numPlanes = frustum.numPlanes;

	bool outsidePlane[6];

	for (int i = 0; i < numPlanes; i++) {

		bool pointsCulled[8] = { true };
		const idPlane plane = frustum.planes[i];

		for (int j = 0; j < 8; j++) {
			const float distance = plane.Distance(corners[j]);
			pointsCulled[j] = distance < 0;			
		}

		outsidePlane[i] = true;
		for (int j = 0; j < 8; j++) {			
			if(!pointsCulled[j]) {
				outsidePlane[i] = false;
			}
		}
	}

	for (int i = 0; i < numPlanes; i++) {
		if(outsidePlane[i])
			return true;
	}

	return false;
}

/*
===================
R_MakeShadowMapFrustums

Called at definition derivation time.
This is very similar to R_MakeShadowFrustums for stencil shadows,
but frustrum for shadow maps need to bet setup slightly differently if the center
of the light is not at (0,0,0).
===================
*/
void R_MakeShadowMapFrustums( idRenderLightLocal *light ) {
	if (light->parms.pointLight) {
		static int	faceCorners[6][4] = {
			{ 7, 5, 1, 3 },		// positive X side
			{ 4, 6, 2, 0 },		// negative X side
			{ 6, 7, 3, 2 },		// positive Y side
			{ 5, 4, 0, 1 },		// negative Y side
			{ 6, 4, 5, 7 },		// positive Z side
			{ 3, 1, 0, 2 }		// negative Z side
		};
		static int	faceEdgeAdjacent[6][4] = {
			{ 4, 4, 2, 2 },		// positive X side
			{ 7, 7, 1, 1 },		// negative X side
			{ 5, 5, 0, 0 },		// positive Y side
			{ 6, 6, 3, 3 },		// negative Y side
			{ 0, 0, 3, 3 },		// positive Z side
			{ 5, 5, 6, 6 }		// negative Z side
		};

		bool	centerOutside = false;

		// if the light center of projection is outside the light bounds,
		// we will need to build the planes a little differently
		if (fabs( light->parms.lightCenter[0] ) > light->parms.lightRadius[0]
			|| fabs( light->parms.lightCenter[1] ) > light->parms.lightRadius[1]
			|| fabs( light->parms.lightCenter[2] ) > light->parms.lightRadius[2]) {
			centerOutside = true;
		}		

		// make the corners to calculate backplane
		idVec3 corners[8];

		for (int i = 0; i < 8; i++) {
			idVec3	temp;
			for (int j = 0; j < 3; j++) {
				if (i & (1 << j)) {
					temp[j] = light->parms.lightRadius[j];
				}
				else {
					temp[j] = -light->parms.lightRadius[j];
				}
			}

			// transform to global space
			corners[i] = light->parms.origin + light->parms.axis * temp;
		}

		//make corners to make side planes of view frustrums
		float xialDist = R_EXP_CalcLightAxialSize(light);
		idVec3 corners2[8];
		for (int i = 0; i < 8; i++) {
			idVec3	temp;
			for (int j = 0; j < 3; j++) {
				if (i & (1 << j)) {
					temp[j] = xialDist;
				}
				else {
					temp[j] = -xialDist;
				}
			}

			// transform to global space
			corners2[i] = light->parms.origin + light->parms.axis * temp + light->parms.axis * light->parms.lightCenter;
		}

		light->numShadowMapFrustums = 0;
		for (int side = 0; side < 6; side++) {
			shadowFrustum_t	*frust = &light->shadowMapFrustums[light->numShadowMapFrustums];

			idVec3 &p1 = corners[faceCorners[side][0]];
			idVec3 &p2 = corners[faceCorners[side][1]];
			idVec3 &p3 = corners[faceCorners[side][2]];
			idPlane backPlane;

			// plane will have positive side inward
			backPlane.FromPoints( p1, p2, p3 );

			// if center of projection is on the wrong side, skip
			float d = backPlane.Distance( light->globalLightOrigin );
			if (d < 0) {
				continue;
			}

			frust->numPlanes = 6;
			frust->planes[5] = backPlane;
			frust->planes[4] = backPlane;	// we don't really need the extra plane

			// make planes with positive side facing inwards in light local coordinates
			for (int edge = 0; edge < 4; edge++) {
				idVec3 &p1 = corners2[faceCorners[side][edge]];
				idVec3 &p2 = corners2[faceCorners[side][(edge + 1) & 3]];

				// create a plane that goes through the center of projection
				frust->planes[edge].FromPoints( p2, p1, light->globalLightOrigin );

#if 0 //???
				// see if we should use an adjacent plane instead
				if (centerOutside) {
					idVec3 &p3 = corners[faceEdgeAdjacent[side][edge]];
					idPlane sidePlane;

					sidePlane.FromPoints( p2, p1, p3 );
					d = sidePlane.Distance( light->globalLightOrigin );
					if (d < 0) {
						// use this plane instead of the edged plane
						frust->planes[edge] = sidePlane;
					}
					// we can't guarantee a neighbor, so add sill planes at edge
					light->shadowMapFrustums[light->numShadowFrustums].makeClippedPlanes = true;
				}
#endif
			}
			light->numShadowMapFrustums++;
		}

		return;
	}

	// projected light

	light->numShadowMapFrustums = 1;
	shadowFrustum_t	*frust = &light->shadowMapFrustums[0];


	// flip and transform the frustum planes so the positive side faces
	// inward in local coordinates

	// it is important to clip against even the near clip plane, because
	// many projected lights that are faking area lights will have their
	// origin behind solid surfaces.
	for (int i = 0; i < 6; i++) {
		idPlane &plane = frust->planes[i];

		plane.SetNormal( -light->frustum[i].Normal() );
		plane.SetDist( -light->frustum[i].Dist() );
	}

	frust->numPlanes = 6;

	frust->makeClippedPlanes = true;
	// projected lights don't have shared frustums, so any clipped edges
	// right on the planes must have a sil plane created for them
}


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

	void AddInteractions( viewLight_t* vlight, const shadowFrustum_t* shadowFrustrums, int numShadowFrustrums ) {
		assert(numShadowFrustrums <= 6);
		assert(numShadowFrustrums >= 0);

		if(vlight->lightDef->lightHasMoved && r_smSkipMovingLights.GetBool()) {
			return;
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

			if (r_smObjectCulling.GetBool() && numShadowFrustrums > 0) {
				visibleSides = 0;

				// cull the entire entity bounding box
				// has referenceBounds been tightened to the actual model bounds?
				idVec3	corners[8];
				for (int i = 0; i < 8; i++) {
					idVec3 tmp;
					tmp[0] = entityDef->referenceBounds[i & 1][0];
					tmp[1] = entityDef->referenceBounds[(i >> 1) & 1][1];
					tmp[2] = entityDef->referenceBounds[(i >> 2) & 1][2];
					R_LocalPointToGlobal(entityDef->modelMatrix, tmp, corners[i]);
				}

				for(int i=0; i<numShadowFrustrums; ++i) {
					if(!R_Cull(corners, shadowFrustrums[i])) {
						visibleSides |= (1 << i);
					}
				}				
			}

			if(!visibleSides)
				continue;

			const int num = inter->numSurfaces;
			for (int i = 0; i < num; i++) {
				const auto& surface = inter->surfaces[i];
				const auto* material = surface.shader;				

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
			
			backEnd.stats.groups[backEndGroup::ShadowMap0 + lod].drawcalls += 1;			
			backEnd.stats.groups[backEndGroup::ShadowMap0 + lod].tris += drawShadow.tris->numIndexes / 3;
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


//offsets to pack 6 smaller textures into a texture atlas (3x2)
static const idVec2 sideOffsets[] = {
	idVec2(0,       0),
	idVec2(1.0/3.0, 0),
	idVec2(1.0/1.5, 0),
	idVec2(0,       0.5),
	idVec2(1.0/3.0, 0.5),
	idVec2(1.0/1.5, 0.5)
};

static void RB_SetupShadowMapViewPort(int side, int lod) {
	const auto* framebuffer = globalImages->shadowmapFramebuffer;

	const idVec2 scale = idVec2( 1.0 / 3.0, 1.0 / 2.0 ) / (1 << lod);
	const idVec2 offset = sideOffsets[side];
	const int width = framebuffer->GetWidth() * scale.x;
	const int height = framebuffer->GetHeight() * scale.y;
	const int offsetX = framebuffer->GetWidth() * offset.x;
	const int offsetY = framebuffer->GetHeight() * offset.y;

	backEnd.shadowCoords[side].scale = scale;
	backEnd.shadowCoords[side].offset = offset;

	glViewport( offsetX, offsetY, width, height );
	glScissor( offsetX, offsetY, width, height );
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

	lod = Max(0, Min(lod, 2));

	const uint64 startTime = Sys_Microseconds();

	// all light side projections must currently match, so non-centered
	// and non-cubic lights must take the largest length
	viewLightAxialSize = R_EXP_CalcLightAxialSize(vLight->lightDef);

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

		ShadowRenderList renderlist;
		renderlist.AddInteractions( vLight, vLight->lightDef->shadowMapFrustums, vLight->lightDef->numShadowMapFrustums );

		glStencilFunc( GL_ALWAYS, 0, 255 );
		GL_State( GLS_DEPTHFUNC_LESS | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );	// make sure depth mask is off before clear
		glDepthMask( GL_TRUE );
		glEnable( GL_DEPTH_TEST );

		globalImages->BindNull( firstShadowMapTextureUnit );
		globalImages->shadowmapFramebuffer->Bind();

		//clear everything in one go
		glViewport(0,0,globalImages->shadowmapFramebuffer->GetWidth(),globalImages->shadowmapFramebuffer->GetHeight());
		glScissor(0,0,globalImages->shadowmapFramebuffer->GetWidth(),globalImages->shadowmapFramebuffer->GetHeight());
		glClearDepth( 1.0 );
		glClear( GL_DEPTH_BUFFER_BIT );

		const int indices = backEnd.pc.c_drawIndexes;

		for (int side = 0; side < 6; side++) {
			RB_SetupShadowMapViewPort( side, lod );

			viewMatrices[side] = fhRenderMatrix::FlipMatrix() * viewMatrices[side];		

			*reinterpret_cast<fhRenderMatrix*>(&backEnd.shadowViewProjection[side][0]) = projectionMatrix * viewMatrices[side];

			renderlist.Submit( viewMatrices[side].ToFloatPtr(), projectionMatrix.ToFloatPtr(), side, lod );
			backEnd.stats.groups[backEndGroup::ShadowMap0 + lod].passes += 1;
		}	

	} else if (vLight->lightDef->parms.parallel) {		
		assert(false && "parallel shadow maps not implemented");
	}
	else {	
		//TODO(johl): get the math straight. this is just terrible and could be done simpler and more efficient
		idVec3 origin = vLight->lightDef->parms.origin;
		idVec3 localTarget = vLight->lightDef->parms.target;
		idVec3 rotatedTarget = vLight->lightDef->parms.axis * vLight->lightDef->parms.target;
		idVec3 rotatedUp = vLight->lightDef->parms.axis * vLight->lightDef->parms.up;
		idVec3 rotatedRight = vLight->lightDef->parms.axis * vLight->lightDef->parms.right;	

		idVec3 worldTarget = vLight->lightDef->parms.origin + rotatedTarget;

		idVec3 flippedOrigin = fhRenderMatrix::FlipMatrix() * origin;
		idVec3 flippedTarget = fhRenderMatrix::FlipMatrix() * worldTarget;
		idVec3 flippedUp     = fhRenderMatrix::FlipMatrix() * rotatedUp;
	
		auto viewMatrix_tmp = fhRenderMatrix::CreateLookAtMatrix( flippedOrigin, flippedTarget, flippedUp );
		auto viewMatrix = viewMatrix_tmp * fhRenderMatrix::FlipMatrix();

		fhRenderMatrix projectionMatrix;
		RB_CreateProjectedProjectionMatrix( vLight, projectionMatrix.ToFloatPtr() );

		ShadowRenderList renderlist;
		renderlist.AddInteractions( vLight, vLight->lightDef->shadowMapFrustums, vLight->lightDef->numShadowMapFrustums );

		glStencilFunc( GL_ALWAYS, 0, 255 );
		GL_State( GLS_DEPTHFUNC_LESS | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO );	// make sure depth mask is off before clear
		glDepthMask( GL_TRUE );
		glEnable( GL_DEPTH_TEST );

		globalImages->BindNull( firstShadowMapTextureUnit );
		globalImages->shadowmapFramebuffer->Bind();

		RB_SetupShadowMapViewPort(0, lod);
		glClearDepth( 1.0 );
		glClear( GL_DEPTH_BUFFER_BIT );

		auto viewProjection = projectionMatrix * viewMatrix;

		memcpy( &backEnd.shadowViewProjection[0][0], viewProjection.ToFloatPtr(), sizeof(float)* 16 );
		memcpy( backEnd.testProjectionMatrix, viewProjection.ToFloatPtr(), sizeof(float)* 16 );
		memcpy( backEnd.testViewMatrix, viewMatrix.ToFloatPtr(), sizeof(float)* 16 );

		renderlist.Submit(viewMatrix.ToFloatPtr(), projectionMatrix.ToFloatPtr(), 0, lod);


		backEnd.stats.groups[backEndGroup::ShadowMap0 + lod].passes += 1;
	}

	globalImages->defaultFramebuffer->Bind();
	globalImages->shadowmapImage->Bind( firstShadowMapTextureUnit );

	glDisable( GL_POLYGON_OFFSET_FILL );	
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW);

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
	backEnd.stats.groups[backEndGroup::ShadowMap0 + lod].time += static_cast<uint32>(endTime - startTime);
}
