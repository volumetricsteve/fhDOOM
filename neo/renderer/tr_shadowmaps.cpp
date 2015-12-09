#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"

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
		GL_ModelViewMatrix.Push();
		GL_ModelViewMatrix.Load(matrix);		

		GL_UseProgram(depthProgram);
		glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
		glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);		

		// draw each surface
		for (int i = 0; i < inter->numSurfaces; i++) {
			surfaceInteraction_t	*surfInt = &inter->surfaces[i];

			if (!surfInt->ambientTris) {
				continue;
			}
			if (surfInt->shader && !surfInt->shader->SurfaceCastsShadow()) {
				continue;
			}

			// cull it
			//if (surfInt->expCulled == CULL_OCCLUDER_AND_RECEIVER) {
//				continue;
			//}

			// render it
			const srfTriangles_t *tri = surfInt->ambientTris;
			if (!tri->ambientCache) {
				R_CreateAmbientCache(const_cast<srfTriangles_t *>(tri), false);
			}

			const auto offset = vertexCache.Bind(tri->ambientCache);
			glVertexAttribPointer(glslProgramDef_t::vertex_attrib_position, 3, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::xyzOffset));
			glVertexAttribPointer(glslProgramDef_t::vertex_attrib_texcoord, 2, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(offset, idDrawVert::texcoordOffset));
			glUniformMatrix4fv(glslProgramDef_t::uniform_modelViewMatrix, 1, false, GL_ModelViewMatrix.Top());
			glUniformMatrix4fv(glslProgramDef_t::uniform_projectionMatrix, 1, false, GL_ProjectionMatrix.Top());

			float color[] {1,0,0,1};
			glUniform4fv(glslProgramDef_t::uniform_diffuse_color, 1, color);
			globalImages->whiteImage->Bind();

			// draw it
			RB_DrawElementsWithCounters(tri);			
		}

		glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
		glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);

		GL_UseProgram(nullptr);

		GL_ModelViewMatrix.Pop();
	}
}

static void RB_RenderShadowBuffer(const viewLight_t* vLight, int side) {


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

	myGlMultMatrix(flippedViewMatrix, lightProjectionMatrix, &backEnd.shadowViewProjection[side][0]);
	RB_RenderShadowCasters(vLight, flippedViewMatrix);


	GL_ProjectionMatrix.Pop();
}


void RB_RenderShadowMaps(const viewLight_t* vLight) {

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


	if (!vLight->frustumTris->ambientCache) {
		R_CreateAmbientCache(const_cast<srfTriangles_t *>(vLight->frustumTris), false);
	}

	for (int side=0; side < 6; side++) {
		// FIXME: check for frustums completely off the screen

		// render a shadow buffer
		RB_RenderShadowBuffer(vLight, side);		
	}

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