#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"
#include "RenderProgram.h"

idCVar r_smObjectCulling( "r_smObjectCulling", "1", CVAR_RENDERER | CVAR_BOOL | CVAR_ARCHIVE, "cull objects/surfaces that are outside the shadow/light frustum when rendering shadow maps" );
idCVar r_smFaceCullMode( "r_smFaceCullMode", "2", CVAR_RENDERER|CVAR_INTEGER | CVAR_ARCHIVE, "Determines which faces should be rendered to shadow map: 0=front, 1=back, 2=front-and-back");
idCVar r_smFov( "r_smFov", "93", CVAR_RENDERER|CVAR_FLOAT | CVAR_ARCHIVE, "fov used when rendering point light shadow maps");
idCVar r_smFarClip( "r_smFarClip", "-1", CVAR_RENDERER|CVAR_INTEGER | CVAR_ARCHIVE, "far clip distance for rendering shadow maps: -1=infinite-z, 0=max light radius, other=fixed distance");
idCVar r_smNearClip( "r_smNearClip", "1", CVAR_RENDERER|CVAR_INTEGER | CVAR_ARCHIVE, "near clip distance for rendering shadow maps");
idCVar r_smUseStaticOccluderModel( "r_smUseStaticOccluderModel", "1", CVAR_RENDERER | CVAR_BOOL | CVAR_ARCHIVE, "the occluder model is a single surface merged from all static and opaque world surfaces. Can be rendered to the shadow map with a single draw call");

idCVar r_smLod( "r_smQuality", "-1", CVAR_RENDERER | CVAR_INTEGER | CVAR_ARCHIVE, "" );

idCVar r_smPolyOffsetFactor( "r_smPolyOffsetFactor", "8", CVAR_RENDERER | CVAR_FLOAT | CVAR_ARCHIVE, "" );
idCVar r_smPolyOffsetBias( "r_smPolyOffsetBias", "26", CVAR_RENDERER | CVAR_FLOAT | CVAR_ARCHIVE, "" );
idCVar r_smSoftness( "r_smSoftness", "1", CVAR_RENDERER | CVAR_FLOAT | CVAR_ARCHIVE, "" );
idCVar r_smBrightness( "r_smBrightness", "0.15", CVAR_RENDERER | CVAR_FLOAT | CVAR_ARCHIVE, "" );

static const int CULL_RECEIVER = 1;	// still draw occluder, but it is out of the view
static const int CULL_OCCLUDER_AND_RECEIVER = 2;	// the surface doesn't effect the view at all

static float viewLightAxialSize;

/*
 * For programming purposes, OpenGL matrices are 16-value arrays with base 
 * vectors laid out contiguously in memory. The translation components occupy 
 * the 13th, 14th, and 15th elements of the 16-element matrix, where indices are
 * numbered from 1 to 16 as described in section 2.11.2 of the OpenGL 2.1 
 * Specification.
 *
 **/
class fhRenderMatrix final {
public:
	fhRenderMatrix() {
		memset(m, 0, sizeof(m));
		m[0] = 1.0f;
		m[5] = 1.0f;
		m[10] = 1.0f;
		m[15] = 1.0f;
	}

	explicit fhRenderMatrix(const float* arr) {
		memcpy(m, arr, sizeof(m));
	}

	~fhRenderMatrix() = default;

	fhRenderMatrix(const fhRenderMatrix& rhs) {
		memcpy(m, rhs.m, sizeof(m));
	}

	const fhRenderMatrix& operator=(const fhRenderMatrix& rhs) {
		memcpy(m, rhs.m, sizeof(m));
		return *this;
	}

	fhRenderMatrix operator*(const fhRenderMatrix& rhs) const {
		fhRenderMatrix ret;

		const float* l = ToFloatPtr();
		const float* r = rhs.ToFloatPtr();
		ret[0] = l[0]*r[0] + l[4]*r[1] + l[8]*r[2] + l[12]*r[3];
		ret[1] = l[1]*r[0] + l[5]*r[1] + l[9]*r[2] + l[13]*r[3];
		ret[2] = l[2]*r[0] + l[6]*r[1] + l[10]*r[2] + l[14]*r[3];
		ret[3] = l[3]*r[0] + l[7]*r[1] + l[11]*r[2] + l[15]*r[3];

		ret[4] = l[0] * r[4] + l[4] * r[5] + l[8] * r[6] + l[12] * r[7];
		ret[5] = l[1] * r[4] + l[5] * r[5] + l[9] * r[6] + l[13] * r[7];
		ret[6] = l[2] * r[4] + l[6] * r[5] + l[10] * r[6] + l[14] * r[7];
		ret[7] = l[3] * r[4] + l[7] * r[5] + l[11] * r[6] + l[15] * r[7];

		ret[8] = l[0] * r[8] + l[4] * r[9] + l[8] * r[10] + l[12] * r[11];
		ret[9] = l[1] * r[8] + l[5] * r[9] + l[9] * r[10] + l[13] * r[11];
		ret[10] = l[2] * r[8] + l[6] * r[9] + l[10] * r[10] + l[14] * r[11];
		ret[11] = l[3] * r[8] + l[7] * r[9] + l[11] * r[10] + l[15] * r[11];

		ret[12] = l[0] * r[12] + l[4] * r[13] + l[8] * r[14] + l[12] * r[15];
		ret[13] = l[1] * r[12] + l[5] * r[13] + l[9] * r[14] + l[13] * r[15];
		ret[14] = l[2] * r[12] + l[6] * r[13] + l[10] * r[14] + l[14] * r[15];
		ret[15] = l[3] * r[12] + l[7] * r[13] + l[11] * r[14] + l[15] * r[15];

		return ret;
	}

	idVec3 operator*(const idVec3& v) const {		
		idVec3 ret;
		ret.x = m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12];
		ret.y = m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13];
		ret.z = m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14];
		return ret;
	}

	idVec4 operator*(const idVec4 v) const {
		idVec4 ret;
		ret.x = m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12] * v.w;
		ret.y = m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13] * v.w;
		ret.z = m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w;
		ret.w = m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w;
		return ret;
	}

	const float& operator[](int index) const {
		assert(index >= 0);
		assert(index < 16);
		return m[index];
	}

	float& operator[]( int index ) {
		assert( index >= 0 );
		assert( index < 16 );
		return m[index];
	}

	const float* ToFloatPtr() const	{
		return &m[0];
	}

	float* ToFloatPtr()	{
		return &m[0];
	}

	static fhRenderMatrix CreateProjectionMatrix(float fov, float aspect, float nearClip, float farClip) {		
		const float D2R = idMath::PI / 180.0;
		const float scale = 1.0 / tan( D2R * fov / 2.0 );
		const float nearmfar = nearClip - farClip;

		fhRenderMatrix m;
		m[0] = scale;
		m[5] = scale;
		m[10] = (farClip + nearClip) / nearmfar;
		m[11] = -1;
		m[14] = 2 * farClip*nearClip / nearmfar;
		m[15] = 0.0f;
		return m;
	}

	static fhRenderMatrix CreateInfiniteProjectionMatrix( float fov, float aspect, float nearClip ) {
		const float ymax = nearClip * tan( fov * idMath::PI / 360.0f );
		const float ymin = -ymax;
		const float xmax = nearClip * tan( fov * idMath::PI / 360.0f );
		const float xmin = -xmax;
		const float width = xmax - xmin;
		const float height = ymax - ymin;

		fhRenderMatrix m;
		memset(&m, 0, sizeof(m));

		m[0] = 2 * nearClip / width;
		m[5] = 2 * nearClip / height;

		// this is the far-plane-at-infinity formulation, and
		// crunches the Z range slightly so w=0 vertexes do not
		// rasterize right at the wraparound point
		m[10] = -0.999f;
		m[11] = -1;
		m[14] = -2.0f * nearClip;
		return m;
	}

	static fhRenderMatrix CreateLookAtMatrix(const idVec3& viewOrigin, const idVec3& at, const idVec3& up)
	{
		fhRenderMatrix rot = CreateLookAtMatrix(at - viewOrigin, up);

		fhRenderMatrix translate;
		translate[12] = -viewOrigin.x;
		translate[13] = -viewOrigin.y;
		translate[14] = -viewOrigin.z;

		return rot * translate;
	}

	static fhRenderMatrix CreateLookAtMatrix( const idVec3& dir, const idVec3& up )
	{
		idVec3 zaxis = (dir * -1).Normalized();
		idVec3 xaxis = up.Cross( zaxis ).Normalized();
		idVec3 yaxis = zaxis.Cross( xaxis );

		fhRenderMatrix m;
		m[0] = xaxis.x;
		m[1] = yaxis.x;
		m[2] = zaxis.x;

		m[4] = xaxis.y;
		m[5] = yaxis.y;
		m[6] = zaxis.y;

		m[8] = xaxis.z;
		m[9] = yaxis.z;
		m[10] = zaxis.z;
		return m;
	}

	static fhRenderMatrix CreateViewMatrix( const idVec3& origin ) {
		fhRenderMatrix m;
		m[12] = -origin.x;
		m[13] = -origin.y;
		m[14] = -origin.z;
		return m;
	}

	static fhRenderMatrix FlipMatrix() {
		static float flipMatrix[16] = {
			// convert from our coordinate system (looking down X)
			// to OpenGL's coordinate system (looking down -Z)
			0, 0, -1, 0,
			-1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 0, 1
		};

		static const fhRenderMatrix m( flipMatrix );
		return m;
	}	

	static const fhRenderMatrix identity;
private:
	float m[16];
};

const fhRenderMatrix fhRenderMatrix::identity;

static float	s_flipMatrix[16] = {
	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};

static void RB_CreateOrthographicProjectionMatrix( const viewLight_t* vlight, float* m )
{
	idVec3 lightSize = vlight->lightDef->parms.lightRadius;
	const float r = lightSize.x / 2.0f;
	const float l = lightSize.x / -2.0f;
	const float t = lightSize.y / 2.0f;
	const float b = lightSize.y / -2.0f;
	const float f = lightSize.z;
	const float n = 0;

	idVec3 viewOrigin = idVec3( 0, 0, lightSize.z / 2.0f );

	memset( m, 0, sizeof(m[0]) * 16 );
	m[0] = 2.0f/(r-l);
	m[5] = 2.0f/(b-t);
	m[10] = -2.0f/(f-n);
	m[12] = -((r+l)/(r-l));
	m[13] = -((t+b)/(t-b));
	m[14] = -((f+n)/(f-n));
	m[15] = 1.0f;
}

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

		if (r_smObjectCulling.GetBool()) {

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

static void RB_RenderShadowCasters(const viewLight_t *vLight, const float* shadowViewMatrix, const float* shadowProjectionMatrix) {	
	
	fhRenderProgram::SetProjectionMatrix( shadowProjectionMatrix );
	fhRenderProgram::SetViewMatrix( shadowViewMatrix );		

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

		bool staticOccluderModelWasRendered = false;
		if(r_smUseStaticOccluderModel.GetBool() && entityDef->staticOccluderModel) {
			if(!r_ignore2.GetBool()) {
				assert(entityDef->staticOccluderModel->NumSurfaces() > 0);

				srfTriangles_t* tri = entityDef->staticOccluderModel->Surface(0)->geometry;

				if (!tri->ambientCache) {
					if (!R_CreateAmbientCache( const_cast<srfTriangles_t *>(tri), false )) {
						common->Error( "RB_RenderShadowCasters: Failed to alloc ambient cache" );
					}
				}

				const auto offset = vertexCache.Bind( tri->ambientCache );		
				GL_SetupVertexAttributes(fhVertexLayout::DrawPosTexOnly, offset);

				fhRenderProgram::SetModelMatrix(fhRenderMatrix::identity.ToFloatPtr());				
				fhRenderProgram::SetAlphaTestEnabled( false );				
				RB_DrawElementsWithCounters( tri );
				backEnd.pc.c_shadowMapDraws++;
			}
			staticOccluderModelWasRendered = true;
		}		

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

			if (staticOccluderModelWasRendered && shader->Coverage() == MC_OPAQUE)
				continue;


			if (!tri->ambientCache) {
				R_CreateAmbientCache(const_cast<srfTriangles_t *>(tri), false);
			}

			const auto offset = vertexCache.Bind(tri->ambientCache);
			GL_SetupVertexAttributes(fhVertexLayout::DrawPosTexOnly, offset);
			fhRenderProgram::SetModelMatrix(inter->entityDef->modelMatrix);		

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

					idVec4 textureMatrix[2] = {idVec4(1,0,0,0), idVec4(0,1,0,0)};
					if (pStage->texture.hasMatrix) {						
						RB_GetShaderTextureMatrix(regs, &pStage->texture, textureMatrix);
					}
					fhRenderProgram::SetDiffuseMatrix(textureMatrix[0], textureMatrix[1]);

					// draw it
					RB_DrawElementsWithCounters(tri);
					backEnd.pc.c_shadowMapDraws++;
					didDraw = true;
					break;
				}
			}
			
			if(!didDraw) {
				fhRenderProgram::SetAlphaTestEnabled(false);
				// draw it
				RB_DrawElementsWithCounters(tri);
				backEnd.pc.c_shadowMapDraws++;
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


static void RB_RenderParallelShadowBuffer( viewLight_t* vLight, int lod ) {
	float	viewMatrix[16];
	memset( viewMatrix, 0, sizeof(viewMatrix) );
	viewMatrix[0] = 1.0f;
	viewMatrix[5] = 1.0f;
	viewMatrix[10] = 1.0f;
	viewMatrix[12] = -vLight->lightDef->parms.origin.x;
	viewMatrix[13] = -vLight->lightDef->parms.origin.y;
	viewMatrix[14] = -vLight->lightDef->parms.origin.z;
	viewMatrix[15] = 1.0f;

	float lightProjectionMatrix[16];
	RB_CreateOrthographicProjectionMatrix( vLight, lightProjectionMatrix );

	fhFramebuffer* framebuffer = globalImages->GetShadowMapFramebuffer(0, lod);
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

	float flippedViewMatrix[16];
	myGlMultMatrix( viewMatrix, s_flipMatrix, flippedViewMatrix );
//	myGlMultMatrix( flippedViewMatrix, lightProjectionMatrix, &backEnd.shadowViewProjection[0][0] );
	RB_RenderShadowCasters( vLight, flippedViewMatrix, lightProjectionMatrix );

	backEnd.pc.c_shadowPasses++;
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

	fhFramebuffer* framebuffer = globalImages->GetShadowMapFramebuffer(0, lod);
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

	RB_RenderShadowCasters( vLight, viewMatrix.ToFloatPtr(), projectionMatrix.ToFloatPtr() );

	backEnd.pc.c_shadowPasses++;
}

static void RB_RenderPointLightShadowBuffer(viewLight_t* vLight, int side, int lod) {
	
	fhRenderMatrix viewMatrix = RB_CreateShadowViewMatrix( vLight, side );

	fhRenderMatrix projectionMatrix;
	if (r_smFarClip.GetInteger() < 0) {
		projectionMatrix = fhRenderMatrix::CreateInfiniteProjectionMatrix(r_smFov.GetFloat(), 1, r_smNearClip.GetFloat());
	} else if(r_smFarClip.GetInteger() == 0) {
		projectionMatrix = fhRenderMatrix::CreateProjectionMatrix(r_smFov.GetFloat(), 1, r_smNearClip.GetFloat(), vLight->lightDef->GetMaximumCenterToEdgeDistance());
	} else {
		projectionMatrix = fhRenderMatrix::CreateProjectionMatrix(r_smFov.GetFloat(), 1, r_smNearClip.GetFloat(), r_smFarClip.GetFloat());
	}

	fhFramebuffer* framebuffer = globalImages->GetShadowMapFramebuffer(side, lod);
	framebuffer->Bind();

	glViewport(0, 0, framebuffer->GetWidth(), framebuffer->GetHeight());
	glScissor(0, 0, framebuffer->GetWidth(), framebuffer->GetHeight());

	glStencilFunc(GL_ALWAYS, 0, 255);

	GL_State(GLS_DEPTHFUNC_LESS | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ZERO);	// make sure depth mask is off before clear
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);

	glClearDepth(1.0);
	glClear(GL_DEPTH_BUFFER_BIT);

	// draw all the occluders	

	backEnd.currentSpace = NULL;

	
	if(r_smObjectCulling.GetBool()) {
		// create frustum planes
		idPlane	globalFrustum[6];
		idVec3	origin = vLight->lightDef->globalLightOrigin;

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


	viewMatrix = fhRenderMatrix::FlipMatrix() * viewMatrix;

    myGlMultMatrix(viewMatrix.ToFloatPtr(), projectionMatrix.ToFloatPtr(), &backEnd.shadowViewProjection[side][0]);
	RB_RenderShadowCasters(vLight, viewMatrix.ToFloatPtr(), projectionMatrix.ToFloatPtr());

	backEnd.pc.c_shadowPasses++;
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
	lod = idMath::ClampInt(0, idImageManager::shadowMapLodNum-1, lod);

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

	static const int firstShadowMapTextureUnit = 6;

	if(vLight->lightDef->parms.pointLight) {
		for (int side=0; side < 6; side++) {
			const int textureUnit = firstShadowMapTextureUnit + side;

			globalImages->BindNull( textureUnit );
			RB_RenderPointLightShadowBuffer(vLight, side, lod);
			globalImages->GetShadowMapImage( side, lod )->Bind( textureUnit );
		}
	} else if (vLight->lightDef->parms.parallel) {
		globalImages->BindNull( firstShadowMapTextureUnit );
		RB_RenderParallelShadowBuffer(vLight, lod);		
		globalImages->GetShadowMapImage( 0, lod )->Bind( firstShadowMapTextureUnit );
	}
	else {	
		globalImages->BindNull( firstShadowMapTextureUnit );
		RB_RenderProjectedShadowBuffer( vLight, lod );		
		globalImages->GetShadowMapImage( 0, lod )->Bind( firstShadowMapTextureUnit );
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
}
