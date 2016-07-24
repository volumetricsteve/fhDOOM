#pragma once

#include "Material.h"

enum class textureSwizzle_t;

class fhSampler {
public:
	fhSampler();
	~fhSampler();

	void Bind(int textureUnit);
	void Purge();

	static fhSampler* GetSampler( textureFilter_t filter, textureRepeat_t repeat, textureSwizzle_t swizzle = textureSwizzle_t::None, bool useAf = true, bool useLodBias = true );

private:
	void Init();

	GLuint				num;
	textureFilter_t		filter;
	textureRepeat_t		repeat;
	textureSwizzle_t	swizzle;
	bool				useAf;
	bool				useLodBias;
};