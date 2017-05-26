/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 2016 Johannes Ohlemacher (http://github.com/eXistence/fhDOOM)

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

#include "Image.h"
#include "Sampler.h"
#include "tr_local.h"

static const int maxSamplerSettings = 128;
static fhSampler samplers[maxSamplerSettings];

fhSampler::fhSampler()
	: num(0)
	, filter(TF_DEFAULT)
	, repeat(TR_REPEAT)
	, swizzle(textureSwizzle_t::None)
	, useAf(true)
	, useLodBias(true) {
}

fhSampler::~fhSampler() {
}

void fhSampler::Bind( int textureUnit ) {
	if(num == 0) {
		Init();
	}

	if(backEnd.glState.tmu[textureUnit].currentSampler != num) {
		glBindSampler(textureUnit, num);
		backEnd.glState.tmu[textureUnit].currentSampler = num;
	}
}

fhSampler* fhSampler::GetSampler( textureFilter_t filter, textureRepeat_t repeat, textureSwizzle_t swizzle, bool useAf, bool useLodBias ) {

	int i = 0;
	for(; i < maxSamplerSettings; ++i) {
		const fhSampler& s = samplers[i];

		if(s.num == 0)
			break;

		if(s.filter != filter)
			continue;

		if (s.repeat != repeat)
			continue;

		if (s.useAf != useAf)
			continue;

		if (s.useLodBias != useLodBias)
			continue;

		if (s.swizzle != swizzle)
			continue;

		return &samplers[i];
	}

	if(i == maxSamplerSettings)
		return nullptr;

	fhSampler& sampler = samplers[i];
	sampler.filter = filter;
	sampler.repeat = repeat;
	sampler.useAf = useAf;
	sampler.useLodBias = useLodBias;
	sampler.swizzle = swizzle;

	sampler.Init();

	return &sampler;
}

void fhSampler::Purge() {
	if(num > 0)
		glDeleteSamplers(1, &num);

	num = 0;
}

void fhSampler::Init() {
	if(num <= 0) {
		glGenSamplers( 1, &num );
	}

	switch (filter) {
	case TF_DEFAULT:
		glSamplerParameteri( num, GL_TEXTURE_MIN_FILTER, globalImages->textureMinFilter );
		glSamplerParameteri( num, GL_TEXTURE_MAG_FILTER, globalImages->textureMaxFilter );
		break;
	case TF_LINEAR:
		glSamplerParameteri( num, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glSamplerParameteri( num, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		break;
	case TF_NEAREST:
		glSamplerParameteri( num, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glSamplerParameteri( num, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		break;
	default:
		common->FatalError( "fhSampler: bad texture filter" );
	}

	switch (repeat) {
	case TR_REPEAT:
		glSamplerParameteri( num, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glSamplerParameteri( num, GL_TEXTURE_WRAP_T, GL_REPEAT );
		break;
	case TR_CLAMP_TO_BORDER:
		glSamplerParameteri( num, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER );
		glSamplerParameteri( num, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER );
		break;
	case TR_CLAMP_TO_ZERO:
	case TR_CLAMP_TO_ZERO_ALPHA:
	case TR_CLAMP:
		glSamplerParameteri( num, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glSamplerParameteri( num, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		break;
	default:
		common->FatalError( "fhSampler: bad texture repeat" );
	}
		
	static const GLint swizzle_AGBR[] = { GL_ALPHA, GL_GREEN, GL_BLUE, GL_RED };

	switch (swizzle) {
	case textureSwizzle_t::AGBR:		
		glSamplerParameteriv( GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle_AGBR );
		break;
	case textureSwizzle_t::None:
		//do nothing
		break;
	default:
		common->FatalError( "fhSampler: bad texture swizzle" );
	}	

	if (glConfig.anisotropicAvailable) {
		// only do aniso filtering on mip mapped images
		if (filter == TF_DEFAULT && useAf) {
			glSamplerParameterf( num, GL_TEXTURE_MAX_ANISOTROPY_EXT, globalImages->textureAnisotropy );
		}
		else {
			glSamplerParameterf( num, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1 );;
		}
	}

	if(useLodBias) {
		glSamplerParameterf( num, GL_TEXTURE_LOD_BIAS, globalImages->textureLODBias );
	} else {
		glSamplerParameterf( num, GL_TEXTURE_LOD_BIAS, 0 );
	}
}