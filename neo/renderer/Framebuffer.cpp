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

#include "Framebuffer.h"
#include "Image.h"
#include "tr_local.h"

fhFramebuffer* fhFramebuffer::currentDrawBuffer = nullptr;
fhFramebuffer* fhFramebuffer::shadowmapFramebuffer = nullptr;
fhFramebuffer* fhFramebuffer::defaultFramebuffer = nullptr;
fhFramebuffer* fhFramebuffer::renderFramebuffer = nullptr;
fhFramebuffer* fhFramebuffer::currentDepthFramebuffer = nullptr;
fhFramebuffer* fhFramebuffer::currentRenderFramebuffer = nullptr;

fhFramebuffer::fhFramebuffer( int w, int h, idImage* color, idImage* depth ) {
	width = w;
	height = h;
	name = (!color && !depth) ? 0 : -1;
	colorAttachment = color;
	depthAttachment = depth;
}

void fhFramebuffer::Bind() {
	if (currentDrawBuffer == this) {
		return;
	}

	if (name == -1) {
		glGenFramebuffers( 1, &name );
		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, name );
		currentDrawBuffer = this;

		if (colorAttachment)
			colorAttachment->AttachColorToFramebuffer( this );

		if (depthAttachment)
			depthAttachment->AttachDepthToFramebuffer( this );

		SetDrawBuffer();

		if (glCheckFramebufferStatus( GL_DRAW_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE) {
			common->Warning( "failed to generate framebuffer, framebuffer incomplete" );
			name = 0;
		}
	}
	else {
		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, name );
		currentDrawBuffer = this;
		SetDrawBuffer();
	}
}

bool fhFramebuffer::IsDefault() const {
	return (name == 0);
}

void fhFramebuffer::Resize( int width, int height ) {
	if (!colorAttachment && !depthAttachment) {
		return;
	}

	if (this->width == width && this->height == height) {
		return;
	}

	const bool isCurrent = (GetCurrentDrawBuffer() == this);
	if (isCurrent) {
		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );
	}

	Purge();

	this->width = width;
	this->height = height;

	if (isCurrent) {
		Bind();
	}
}

int fhFramebuffer::GetWidth() const {
	if (!colorAttachment && !depthAttachment) {
		return glConfig.vidWidth;
	}
	return width;
}

int fhFramebuffer::GetHeight() const {
	if (!colorAttachment && !depthAttachment) {
		return glConfig.vidHeight;
	}
	return height;
}


void fhFramebuffer::Purge() {
	if (name != 0 && name != -1) {
		glDeleteFramebuffers( 1, &name );
		name = -1;
	}
}

void fhFramebuffer::BlitToCurrentFramebuffer() {
	if (currentDrawBuffer && currentDrawBuffer != this) {
		glBindFramebuffer( GL_READ_FRAMEBUFFER, name );
		glReadBuffer( GL_COLOR_ATTACHMENT0 );

		const int src_x2 = width;
		const int src_y2 = height;

		const int dst_x2 = currentDrawBuffer->GetWidth();
		const int dst_y2 = currentDrawBuffer->GetHeight();

		if (src_x2 != dst_x2 ||
			src_y2 != dst_y2) {
			common->Warning( "size mismatch!?" );
		}

		glBlitFramebuffer(
			0, 0, src_x2, src_y2,
			0, 0, dst_x2, dst_y2,
			GL_COLOR_BUFFER_BIT, GL_LINEAR );

		glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );
	}
	else {
		common->Warning( "no current framebuffer!?" );
	}
}

void fhFramebuffer::BlitDepthToCurrentFramebuffer() {
	if (currentDrawBuffer && currentDrawBuffer != this) {
		glBindFramebuffer( GL_READ_FRAMEBUFFER, name );
		glReadBuffer( GL_NONE );

		const int src_x2 = width;
		const int src_y2 = height;

		const int dst_x2 = currentDrawBuffer->GetWidth();
		const int dst_y2 = currentDrawBuffer->GetHeight();

		if (src_x2 != dst_x2 ||
			src_y2 != dst_y2) {
			common->Warning( "size mismatch!?" );
		}

		glBlitFramebuffer(
			0, 0, src_x2, src_y2,
			0, 0, dst_x2, dst_y2,
			GL_DEPTH_BUFFER_BIT, GL_NEAREST );

		glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );
	}
	else {
		common->Warning( "no current framebuffer!?" );
	}
}



void fhFramebuffer::SetDrawBuffer() {
	if (!colorAttachment && !depthAttachment) {
		glDrawBuffer( GL_BACK );
	}
	else if (!colorAttachment) {
		glDrawBuffer( GL_NONE );
	}
	else {
		glDrawBuffer( GL_COLOR_ATTACHMENT0 );
	}
}

void fhFramebuffer::Init() {
	shadowmapFramebuffer = new fhFramebuffer( 1024 * 4, 1024 * 4, nullptr, globalImages->shadowmapImage );
	defaultFramebuffer = new fhFramebuffer( 0, 0, nullptr, nullptr );
	renderFramebuffer = new fhFramebuffer( 1024, 1024, globalImages->renderColorImage, globalImages->renderDepthImage );
	currentDepthFramebuffer = new fhFramebuffer( 1024, 1024, nullptr, globalImages->currentDepthImage );
	currentRenderFramebuffer = new fhFramebuffer( 1024, 1024, globalImages->currentRenderImage, nullptr );
}
