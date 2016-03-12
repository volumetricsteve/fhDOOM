#pragma once

struct fhRenderProgram {
	static const int vertex_attrib_position = 0;
	static const int vertex_attrib_texcoord = 1;
	static const int vertex_attrib_normal = 2;
	static const int vertex_attrib_color = 3;
	static const int vertex_attrib_binormal = 4;
	static const int vertex_attrib_tangent = 5;
	static const int vertex_attrib_position_shadow = 6;

	static const int uniform_modelMatrix = 0;
	static const int uniform_viewMatrix = 1;
	static const int uniform_modelViewMatrix = 2;
	static const int uniform_projectionMatrix = 3;

	static const int uniform_localLightOrigin = 4;
	static const int uniform_localViewOrigin = 5;

	static const int uniform_lightProjectionMatrixS = 6;
	static const int uniform_lightProjectionMatrixT = 7;
	static const int uniform_lightProjectionMatrixQ = 8;
	static const int uniform_lightFallOffS = 9;

	static const int uniform_bumpMatrixS = 10;
	static const int uniform_bumpMatrixT = 11;
	static const int uniform_diffuseMatrixS = 12;
	static const int uniform_diffuseMatrixT = 13;
	static const int uniform_specularMatrixS = 14;
	static const int uniform_specularMatrixT = 15;

	static const int uniform_color_modulate = 16;
	static const int uniform_color_add = 17;

	static const int uniform_diffuse_color = 18;
	static const int uniform_specular_color = 19;

	static const int uniform_shaderparm0 = 20;
	static const int uniform_shaderparm1 = uniform_shaderparm0 + 1;
	static const int uniform_shaderparm2 = uniform_shaderparm0 + 2;
	static const int uniform_shaderparm3 = uniform_shaderparm0 + 3;

	static const int uniform_texture0 = 24;
	static const int uniform_texture1 = uniform_texture0 + 1;
	static const int uniform_texture2 = uniform_texture0 + 2;
	static const int uniform_texture3 = uniform_texture0 + 3;
	static const int uniform_texture4 = uniform_texture0 + 4;
	static const int uniform_texture5 = uniform_texture0 + 5;
	static const int uniform_texture6 = uniform_texture0 + 6;
	static const int uniform_texture7 = uniform_texture0 + 7;
	static const int uniform_texture8 = uniform_texture0 + 8;
	static const int uniform_texture9 = uniform_texture0 + 9;
	static const int uniform_texture10 = uniform_texture0 + 10;
	static const int uniform_texture11 = uniform_texture0 + 11;
	static const int uniform_texture12 = uniform_texture0 + 12;
	static const int uniform_texture13 = uniform_texture0 + 13;
	static const int uniform_texture14 = uniform_texture0 + 14;
	static const int uniform_texture15 = uniform_texture0 + 15;

	static const int uniform_textureMatrix0 = 50;

	static const int uniform_alphaTestEnabled = 100;
	static const int uniform_alphaTestThreshold = 101;
	static const int uniform_currentRenderSize = 102;

	static const int uniform_clipRange = 103;
	static const int uniform_depthBlendMode = 104;
	static const int uniform_depthBlendRange = 105;
	static const int uniform_pomMaxHeight = 106;
	static const int uniform_shading = 107;
	static const int uniform_specularExp = 108;
	static const int uniform_specularScale = 109;

	static const int uniform_shadowMappingMode = 120;
	static const int uniform_spotlightProjection = 121;
	static const int uniform_pointlightProjection = 122;

	static const int uniform_globalLightOrigin = 128;
	static const int uniform_shadowParams = 129;
	static const int uniform_shadowSamples = 130;

public:
	fhRenderProgram();
	~fhRenderProgram();

	void Load(const char* vertexShader, const char* fragmentShader);
	void Reload();
	void Purge();
	void Bind() const;

	const char* vertexShader() const;
	const char* fragmentShader() const;

	static void Unbind();
	static void ReloadAll();
	static void PurgeAll();
	static void Init();

private:
	void Load();

	char   vertexShaderName[64];
	char   fragmentShaderName[64];
	GLuint ident;
};

extern const fhRenderProgram* shadowProgram;
extern const fhRenderProgram* interactionProgram;
extern const fhRenderProgram* depthProgram;
extern const fhRenderProgram* shadowmapProgram;
extern const fhRenderProgram* defaultProgram;
extern const fhRenderProgram* skyboxProgram;
extern const fhRenderProgram* bumpyEnvProgram;
extern const fhRenderProgram* fogLightProgram;
extern const fhRenderProgram* vertexColorProgram;
extern const fhRenderProgram* flatColorProgram;
extern const fhRenderProgram* intensityProgram;
extern const fhRenderProgram* blendLightProgram;
extern const fhRenderProgram* depthblendProgram;