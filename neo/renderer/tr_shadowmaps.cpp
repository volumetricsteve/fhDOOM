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

idCVar r_smCascadeDistance0( "r_smCascadeDistance0", "150", CVAR_RENDERER | CVAR_FLOAT | CVAR_ARCHIVE, "" );
idCVar r_smCascadeDistance1( "r_smCascadeDistance1", "300", CVAR_RENDERER | CVAR_FLOAT | CVAR_ARCHIVE, "" );
idCVar r_smCascadeDistance2( "r_smCascadeDistance2", "500", CVAR_RENDERER | CVAR_FLOAT | CVAR_ARCHIVE, "" );
idCVar r_smCascadeDistance3( "r_smCascadeDistance3", "800", CVAR_RENDERER | CVAR_FLOAT | CVAR_ARCHIVE, "" );
idCVar r_smCascadeDistance4( "r_smCascadeDistance4", "1200", CVAR_RENDERER | CVAR_FLOAT | CVAR_ARCHIVE, "" );
idCVar r_smViewDependendCascades( "r_smViewDependendCascades", "6", CVAR_RENDERER | CVAR_INTEGER | CVAR_ARCHIVE, "" );

static const int firstShadowMapTextureUnit = 6;

enum class ShadowMapSize
{
	SM4096,
	SM2048,
	SM1024,
	SM512,
	SM256,
	NUM
};

size_t shadowMapSizes[] {
	4096,
	2048,
	1024,
	512,
	256
};

class fhShadowMapAllocator {
public:
	fhShadowMapAllocator() {
		int size = 1;
		for (int i = 0; i < (int)ShadowMapSize::NUM; ++i) {
			freelist[i].AssureSize(size);
			size *= 4;
		}

		FreeAll();
	}

	bool Allocate( int lod, int num, shadowCoord_t* coords ) {		
		switch(lod) {
		case 0:
			return Allocate( ShadowMapSize::SM1024, num, coords );
		case 1:
			return Allocate( ShadowMapSize::SM512, num, coords );
		case 2:
		default:
			return Allocate( ShadowMapSize::SM256, num, coords );
		}
	}

	void FreeAll() {
		for(int i=0; i<(int)ShadowMapSize::NUM; ++i) {
			freelist[i].SetNum(0);
		}

		freelist[0].Append(shadowCoord_t{idVec2(1,1), idVec2(0,0)});
	}

private:
	bool Allocate( ShadowMapSize size, int num, shadowCoord_t* coords ) {
		for (int i = 0; i < num; ++i) {
			if (!Allocate( size, coords[i] )) {
				return false;
			}
		}

		return true;
	}


	bool Allocate( ShadowMapSize size, shadowCoord_t& coords ) {
		if (!Make( (int)size )) {
			return false;
		}

		idList<shadowCoord_t>& level = freelist[(int)size];
		coords = level[level.Num() - 1];
		level.RemoveIndex(level.Num() - 1);
		return true;
	}

	bool Make( int sizeIndex ) {
		if (freelist[sizeIndex].Num() > 0) {
			return true;
		}

		if (sizeIndex == 0 || !Make( sizeIndex - 1 )) {
			return false;
		}

		idList<shadowCoord_t>& thisLevel = freelist[sizeIndex];
		idList<shadowCoord_t>& prevLevel = freelist[sizeIndex - 1];

		//take entry from upper level
		shadowCoord_t oldCoords = prevLevel[prevLevel.Num() - 1];
		prevLevel.RemoveIndex( prevLevel.Num() - 1 );

		//create 4 new entries in this level
		const idVec2 scale( (float)shadowMapSizes[sizeIndex] / (float)shadowMapSizes[0], (float)shadowMapSizes[sizeIndex] / (float)shadowMapSizes[0] );

		shadowCoord_t newCoords[4];

		newCoords[0].scale = scale;
		newCoords[0].offset = oldCoords.offset + idVec2( 0, 0 );

		newCoords[1].scale = scale;
		newCoords[1].offset = oldCoords.offset + idVec2( scale.x, 0 );

		newCoords[2].scale = scale;
		newCoords[2].offset = oldCoords.offset + idVec2( 0, scale.y );

		newCoords[3].scale = scale;
		newCoords[3].offset = oldCoords.offset + idVec2( scale.x, scale.y );

		for(int i=0; i<4; ++i) {
			thisLevel.Append(newCoords[i]);
		}

		return true;
	}

	void Split( const shadowCoord_t& src, shadowCoord_t* dst, idVec2 scale, float size ) {

		dst[0].scale = scale;
		dst[0].offset = src.offset + idVec2( 0, 0 );

		dst[1].scale = scale;
		dst[1].offset = src.offset + idVec2( size, 0 );

		dst[2].scale = scale;
		dst[2].offset = src.offset + idVec2( 0, size );

		dst[3].scale = scale;
		dst[3].offset = src.offset + idVec2( size, size );
	}

	idList<shadowCoord_t> freelist[(int)ShadowMapSize::NUM];
};

fhShadowMapAllocator shadowMapAllocator;

/*
===================
R_Cull

cull 8 corners against given shadow frustum.
Return true of all 8 corners are outside the frustum.
===================
*/
static bool R_Cull(const idVec3* corners, const shadowMapFrustum_t& frustum) {

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


static void RB_CreateProjectedProjectionMatrix( const idRenderLightLocal* light, float* m )
{
	auto parms = light->parms;

	if (parms.start.LengthSqr() < 0.0001f)
		parms.start = parms.target.Normalized() * 8.0f;

	if (parms.end.LengthSqr() < 0.0001f)
		parms.end = parms.target;

	idPlane planes[4];
	R_SetLightProject( planes, idVec3( 0, 0, 0 ), parms.target, parms.right, parms.up, parms.start, parms.end );

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

static fhRenderMatrix RB_CreatePointLightViewMatrix( const idRenderLightLocal* lightDef, int side )
{
	static const idMat3 sides[6] = {
		idMat3( //+x
			1, 0, 0,
		    0, 1, 0,
			0, 0, 1),
		idMat3( //-x
			-1, 0, 0,
			0, 1, 0,
			0, 0, -1),
		idMat3( //+y
			0, 1, 0,
			-1, 0, 0,
			0, 0, 1),
		idMat3( //-y
			0, -1, 0,
			1, 0, 0,
			0, 0, 1 ),
		idMat3( //z
			0, 0, 1,
			0, 1, 0,
			-1, 0, 0 ),
		idMat3( //-z
			0, 0, -1,
			0, 1, 0,
			1, 0, 0 )
	};

	const idVec3 origin = lightDef->globalLightOrigin;
	const idMat3 axis = sides[side] * lightDef->parms.axis;

	fhRenderMatrix	viewerMatrix;
	viewerMatrix[0] = axis[0][0];
	viewerMatrix[4] = axis[0][1];
	viewerMatrix[8] = axis[0][2];
	viewerMatrix[12] = -origin[0] * viewerMatrix[0] + -origin[1] * viewerMatrix[4] + -origin[2] * viewerMatrix[8];

	viewerMatrix[1] = axis[1][0];
	viewerMatrix[5] = axis[1][1];
	viewerMatrix[9] = axis[1][2];
	viewerMatrix[13] = -origin[0] * viewerMatrix[1] + -origin[1] * viewerMatrix[5] + -origin[2] * viewerMatrix[9];

	viewerMatrix[2] = axis[2][0];
	viewerMatrix[6] = axis[2][1];
	viewerMatrix[10] = axis[2][2];
	viewerMatrix[14] = -origin[0] * viewerMatrix[2] + -origin[1] * viewerMatrix[6] + -origin[2] * viewerMatrix[10];

	viewerMatrix[3] = 0;
	viewerMatrix[7] = 0;
	viewerMatrix[11] = 0;
	viewerMatrix[15] = 1;

	return fhRenderMatrix::FlipMatrix() * viewerMatrix;
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
	if (light->parms.pointLight || light->parms.parallel) {
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
		//all light side projections must currently match, so non-centered
		//	and non-cubic lights must take the largest length
		float farplaneDistance = 0;

		for (int i = 0; i < 3; i++) {
			float	dist = fabs( light->parms.lightCenter[i] );
			dist += light->parms.lightRadius[i];
			if (dist > farplaneDistance) {
				farplaneDistance = dist;
			}
		}

		idVec3 corners2[8];
		for (int i = 0; i < 8; i++) {
			idVec3	temp;
			for (int j = 0; j < 3; j++) {
				if (i & (1 << j)) {
					temp[j] = farplaneDistance;
				}
				else {
					temp[j] = -farplaneDistance;
				}
			}

			// transform to global space
			corners2[i] = light->parms.origin + light->parms.axis * temp + light->parms.axis * light->parms.lightCenter;
		}

		if (light->parms.parallel) {
			light->numShadowMapFrustums = 1;
			shadowMapFrustum_t& frustum = light->shadowMapFrustums[0];

			idVec3 target = -light->parms.lightCenter;
			idVec3 up = idVec3( 1, 0, 0 );

			idVec3 flippedTarget = fhRenderMatrix::FlipMatrix() * target;
			idVec3 flippedUp = fhRenderMatrix::FlipMatrix() * up;

			const auto viewMatrix_tmp = fhRenderMatrix::CreateLookAtMatrix( idVec3( 0, 0, 0 ), flippedTarget, flippedUp );
			frustum.viewMatrix = viewMatrix_tmp * fhRenderMatrix::FlipMatrix();

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

			for (int i = 0; i < 8; i++) {
				corners[i] = frustum.viewMatrix * corners[i];
			}

			frustum.viewSpaceBounds.FromPoints(corners, 8);
		}
		else {
			// all sides share the same projection matrix
			fhRenderMatrix projectionMatrix;

			if (r_smFarClip.GetInteger() < 0) {
				projectionMatrix = fhRenderMatrix::CreateInfiniteProjectionMatrix( r_smFov.GetFloat(), 1, r_smNearClip.GetFloat() );
			}
			else if (r_smFarClip.GetInteger() == 0) {
				projectionMatrix = fhRenderMatrix::CreateProjectionMatrix( r_smFov.GetFloat(), 1, r_smNearClip.GetFloat(), farplaneDistance );
			}
			else {
				projectionMatrix = fhRenderMatrix::CreateProjectionMatrix( r_smFov.GetFloat(), 1, r_smNearClip.GetFloat(), r_smFarClip.GetFloat() );
			}
			
			for (int side = 0; side < 6; side++) {
				shadowMapFrustum_t *frust = &light->shadowMapFrustums[side];
				frust->projectionMatrix = projectionMatrix;
				frust->viewMatrix = RB_CreatePointLightViewMatrix( light, side );
				frust->viewProjectionMatrix = frust->projectionMatrix * frust->viewMatrix;

				frust->farPlaneDistance = farplaneDistance;
				frust->nearPlaneDistance = r_smNearClip.GetFloat();

				const idVec3 &p1 = corners[faceCorners[side][0]];
				const idVec3 &p2 = corners[faceCorners[side][1]];
				const idVec3 &p3 = corners[faceCorners[side][2]];
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
					const idVec3 &p1 = corners2[faceCorners[side][edge]];
					const idVec3 &p2 = corners2[faceCorners[side][(edge + 1) & 3]];

					// create a plane that goes through the center of projection
					frust->planes[edge].FromPoints( p2, p1, light->globalLightOrigin );
				}
			}

			light->numShadowMapFrustums = 6;
		}
	}
	else {
		// projected light
		light->numShadowMapFrustums = 1;
		shadowMapFrustum_t	*frust = &light->shadowMapFrustums[0];


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

		//TODO(johl): get the math straight. this is just terrible and could be done simpler and more efficient
		idVec3 origin = light->parms.origin;
		idVec3 localTarget = light->parms.target;
		idVec3 rotatedTarget = light->parms.axis * light->parms.target;
		idVec3 rotatedUp = light->parms.axis * light->parms.up;
		idVec3 rotatedRight = light->parms.axis * light->parms.right;

		idVec3 worldTarget = light->parms.origin + rotatedTarget;

		idVec3 flippedOrigin = fhRenderMatrix::FlipMatrix() * origin;
		idVec3 flippedTarget = fhRenderMatrix::FlipMatrix() * worldTarget;
		idVec3 flippedUp = fhRenderMatrix::FlipMatrix() * rotatedUp;

		auto viewMatrix_tmp = fhRenderMatrix::CreateLookAtMatrix( flippedOrigin, flippedTarget, flippedUp );
		frust->viewMatrix = viewMatrix_tmp * fhRenderMatrix::FlipMatrix();
		
		RB_CreateProjectedProjectionMatrix( light, frust->projectionMatrix.ToFloatPtr() );

		frust->viewProjectionMatrix = frust->projectionMatrix * frust->viewMatrix;
		frust->nearPlaneDistance = light->parms.start.Length();
		frust->farPlaneDistance = light->parms.end.Length();
	}
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

	void AddInteractions( viewLight_t* vlight, const shadowMapFrustum_t* shadowFrustrums, int numShadowFrustrums ) {
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

		const bool objectCullingEnabled = r_smObjectCulling.GetBool() && (numShadowFrustrums > 0);
		
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

			if (objectCullingEnabled) {
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


bool RB_RenderShadowMaps( viewLight_t* vLight ) {

	const idMaterial* lightShader = vLight->lightShader;
	
	if (lightShader->IsFogLight() || lightShader->IsBlendLight()) {
		return true;
	}

	if (!vLight->localInteractions && !vLight->globalInteractions
		&& !vLight->translucentInteractions) {
		return true;
	}

	if (!vLight->lightShader->LightCastsShadows()) {
		return true;
	}

	int lod = vLight->shadowMapLod;
	if (r_smLod.GetInteger() >= 0) {
		lod = r_smLod.GetInteger();
	}

	lod = Max( 0, Min( lod, 2 ) );

	const uint64 startTime = Sys_Microseconds();

	const float polygonOffsetBias = vLight->lightDef->ShadowPolygonOffsetBias();
	const float polygonOffsetFactor = vLight->lightDef->ShadowPolygonOffsetFactor();
	glEnable( GL_POLYGON_OFFSET_FILL );
	glPolygonOffset( polygonOffsetFactor, polygonOffsetBias );

	switch (r_smFaceCullMode.GetInteger())
	{
	case 0:
		glEnable( GL_CULL_FACE );
		glFrontFace( GL_CCW );
		break;
	case 1:
		glEnable( GL_CULL_FACE );
		glFrontFace( GL_CW );
		break;
	case 2:
	default:
		glDisable( GL_CULL_FACE );
		break;
	}

	ShadowRenderList renderlist;
	int numShadowMaps = 0;

	if (vLight->lightDef->parms.parallel) {
		assert( vLight->lightDef->numShadowMapFrustums == 1 );
		shadowMapFrustum_t& frustum = vLight->lightDef->shadowMapFrustums[0];

		if (!shadowMapAllocator.Allocate( 0, 6, vLight->shadowCoords )) {
			return false;
		}

		const float cascadeDistances[6] = {
			r_smCascadeDistance0.GetFloat(),
			r_smCascadeDistance1.GetFloat(),
			r_smCascadeDistance2.GetFloat(),
			r_smCascadeDistance3.GetFloat(),
			r_smCascadeDistance4.GetFloat(),
			100000
		};

		lod = 0;
		float nextNearDistance = 1;
		for (int c = 0; c < 6; ++c) {
			idFrustum viewFrustum = backEnd.viewDef->viewFrustum;
			viewFrustum.MoveFarDistance( cascadeDistances[c] ); //move far before near, so far is always greater than near
			viewFrustum.MoveNearDistance( nextNearDistance );
			nextNearDistance = cascadeDistances[c];

			idVec3 viewCorners[8];
			viewFrustum.ToPoints( viewCorners );

			for (int i = 0; i < 8; ++i) {
				viewCorners[i] = frustum.viewMatrix * viewCorners[i];
			}

			idVec2 viewMinimum = viewCorners[0].ToVec2();
			idVec2 viewMaximum = viewCorners[0].ToVec2();

			for (int i = 1; i < 8; ++i) {
				const float x = viewCorners[i].x;
				const float y = viewCorners[i].y;

				viewMinimum.x = Min( viewMinimum.x, x );
				viewMinimum.y = Min( viewMinimum.y, y );

				viewMaximum.x = Max( viewMaximum.x, x );
				viewMaximum.y = Max( viewMaximum.y, y );
			}

			idVec2 minimum, maximum;

			if (c < r_smViewDependendCascades.GetInteger()) {
				minimum.x = Max( frustum.viewSpaceBounds[0].x, viewMinimum.x );
				minimum.y = Max( frustum.viewSpaceBounds[0].y, viewMinimum.y );
				maximum.x = Min( frustum.viewSpaceBounds[1].x, viewMaximum.x );
				maximum.y = Min( frustum.viewSpaceBounds[1].y, viewMaximum.y );
			}
			else {
				minimum = frustum.viewSpaceBounds[0].ToVec2();
				maximum = frustum.viewSpaceBounds[1].ToVec2();
			}

			float r = idMath::Abs( maximum.x - minimum.x ) * 0.5f;
			float l = -r;
			float t = idMath::Abs( maximum.y - minimum.y ) * 0.5f;
			float b = -t;

			if (r_ignore.GetBool()) {
				idVec2 vWorldUnitsPerTexel = idVec2( 2 * r, 2 * t ) / 1024.0f;

				minimum /= vWorldUnitsPerTexel;
				minimum.x = idMath::Floor( minimum.x ) - 10;
				minimum.y = idMath::Floor( minimum.y ) - 10;
				minimum *= vWorldUnitsPerTexel;

				maximum /= vWorldUnitsPerTexel;
				maximum.x = idMath::Ceil( maximum.x ) + 10;
				maximum.y = idMath::Ceil( maximum.y ) + 10;
				maximum *= vWorldUnitsPerTexel;
			}

			vLight->viewMatrices[c] = frustum.viewMatrix;
			vLight->viewMatrices[c][12] = -(maximum.x + minimum.x) * 0.5f;
			vLight->viewMatrices[c][13] = -(maximum.y + minimum.y) * 0.5f;
			vLight->viewMatrices[c][14] = -(frustum.viewSpaceBounds[1].z + 1);

			const float f = abs( frustum.viewSpaceBounds[1].z - frustum.viewSpaceBounds[0].z );
			const float n = 1;

			vLight->projectionMatrices[c] = fhRenderMatrix::identity;
			vLight->projectionMatrices[c][0] = 2.0f / (r - l);
			vLight->projectionMatrices[c][5] = 2.0f / (b - t);
			vLight->projectionMatrices[c][10] = -2.0f / (f - n);
			vLight->projectionMatrices[c][12] = -((r + l) / (r - l));
			vLight->projectionMatrices[c][13] = -((t + b) / (t - b));
			vLight->projectionMatrices[c][14] = -((f + n) / (f - n));
			vLight->projectionMatrices[c][15] = 1.0f;

			vLight->viewProjectionMatrices[c] = vLight->projectionMatrices[c] * vLight->viewMatrices[c];
			vLight->width[c] = abs( r * 2 );
			vLight->height[c] = abs( t * 2 );
		}

		renderlist.AddInteractions( vLight, nullptr, 0 );

		numShadowMaps = 6;
	}
	else if (vLight->lightDef->parms.pointLight) {
		assert( vLight->lightDef->numShadowMapFrustums == 6 );

		if(!shadowMapAllocator.Allocate( lod, 6, vLight->shadowCoords )) {
			return false;
		}

		for (int i = 0; i < 6; ++i) {
			vLight->viewMatrices[i] = vLight->lightDef->shadowMapFrustums[i].viewMatrix;
			vLight->projectionMatrices[i] = vLight->lightDef->shadowMapFrustums[i].projectionMatrix;
			vLight->viewProjectionMatrices[i] = vLight->lightDef->shadowMapFrustums[i].viewProjectionMatrix;
		}

		renderlist.AddInteractions( vLight, vLight->lightDef->shadowMapFrustums, vLight->lightDef->numShadowMapFrustums );

		numShadowMaps = 6;
	}
	else {
		//projected light

		if (!shadowMapAllocator.Allocate( lod, 1, vLight->shadowCoords )) {
			return false;
		}

		assert( vLight->lightDef->numShadowMapFrustums == 1 );

		vLight->viewMatrices[0] = vLight->lightDef->shadowMapFrustums[0].viewMatrix;
		vLight->projectionMatrices[0] = vLight->lightDef->shadowMapFrustums[0].projectionMatrix;
		vLight->viewProjectionMatrices[0] = vLight->lightDef->shadowMapFrustums[0].viewProjectionMatrix;

		renderlist.AddInteractions( vLight, &vLight->lightDef->shadowMapFrustums[0], 1 );

		numShadowMaps = 1;
	}

	for (int side = 0; side < numShadowMaps; side++) {

		const fhFramebuffer* framebuffer = globalImages->shadowmapFramebuffer;

		const int width = framebuffer->GetWidth() * vLight->shadowCoords[side].scale.x;
		const int height = framebuffer->GetHeight() * vLight->shadowCoords[side].scale.y;
		const int offsetX = framebuffer->GetWidth() * vLight->shadowCoords[side].offset.x;
		const int offsetY = framebuffer->GetHeight() * vLight->shadowCoords[side].offset.y;

		glViewport( offsetX, offsetY, width, height );
		glScissor( offsetX, offsetY, width, height );

		renderlist.Submit( vLight->viewMatrices[side].ToFloatPtr(), vLight->projectionMatrices[side].ToFloatPtr(), side, lod );
		backEnd.stats.groups[backEndGroup::ShadowMap0 + lod].passes += 1;
	}



	const uint64 endTime = Sys_Microseconds();
	backEnd.stats.groups[backEndGroup::ShadowMap0 + lod].time += static_cast<uint32>(endTime - startTime);

	return true;
}

void RB_FreeAllShadowMaps() {
	shadowMapAllocator.FreeAll();
}