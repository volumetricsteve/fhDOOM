#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"
#include "ImmediateMode.h"

namespace {
  const int drawVertsCapacity = 1024;
  idDrawVert drawVerts[drawVertsCapacity];
  unsigned short lineIndices[drawVertsCapacity * 2];

//  GLuint lineVertexBUffer;
//  GLuint lineIndexBUffer;


}


void fhImmediateMode::Init()
{
  for(int i=0; i<drawVertsCapacity*2; ++i)
  {
    lineIndices[i] = i;
  }
/*
  const size_t vertexSize = sizeof(drawVerts);
  const size_t indexSize = drawVertsCapacity * 2 * sizeof(lineIndices[0]);

  //TODO: vertex buffer should be managed by vertexCache
  glGenBuffers(1, &lineVertexBUffer);
  glBindBuffer( GL_ARRAY_BUFFER, lineVertexBUffer );
  glBufferData( GL_ARRAY_BUFFER, (GLsizeiptrARB)vertexSize, nullptr, GL_STREAM_DRAW );
  glBindBuffer( GL_ARRAY_BUFFER, 0);

  //TODO: index buffer should be managed by some kind of 'indexCache' (similar to vertexCache, does not exist yet)
  glGenBuffers(1, &lineIndexBUffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lineIndexBUffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptrARB)indexSize, linesIndices, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  delete[] linesIndices;
*/
}


fhImmediateMode::fhImmediateMode()
: drawVertsUsed(0)
, currentTexture(nullptr)
{
}

fhImmediateMode::~fhImmediateMode()
{
  End();
}

void fhImmediateMode::SetTexture(idImage* texture)
{
  currentTexture = texture;
}

void fhImmediateMode::Begin(GLenum mode)
{
  End();
  currentMode = mode;
  drawVertsUsed = 0;
}


template<typename T>
static const void* attributeOffset(T offset, const void* attributeOffset)
{
  return reinterpret_cast<const void*>((std::ptrdiff_t)offset + (std::ptrdiff_t)attributeOffset);
}

void fhImmediateMode::End()
{
  if(!drawVertsUsed)
    return;

  if(backEnd.glslEnabled)
  {
    auto vert = vertexCache.AllocFrameTemp(drawVerts, drawVertsUsed * sizeof(idDrawVert));
    int offset = vertexCache.Bind(vert);

    if(currentTexture) {
      GL_SelectTexture(1);
      currentTexture->Bind();
      GL_UseProgram(defaultProgram);
      GL_SelectTexture(0);
    } else {
      GL_UseProgram(vertexColorProgram);
    }

    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);
    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_position, 3, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(0, attributeOffset(offset, idDrawVert::xyzOffset)));
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_color, 4, GL_UNSIGNED_BYTE, false, sizeof(idDrawVert), attributeOffset(0, attributeOffset(offset, idDrawVert::colorOffset)));
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_texcoord, 2, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(0, attributeOffset(offset, idDrawVert::texcoordOffset)));

    glUniformMatrix4fv(glslProgramDef_t::uniform_modelViewMatrix, 1, false, GL_ModelViewMatrix.Top());
    glUniformMatrix4fv(glslProgramDef_t::uniform_projectionMatrix, 1, false, GL_ProjectionMatrix.Top());
    glUniform4f(glslProgramDef_t::uniform_diffuse_color, 1, 1, 1, 1);

    GLenum mode = currentMode;

    
    if(mode == GL_QUADS) //quads are replaced by triangles in GLSL mode
      mode == GL_TRIANGLES;

    glDrawElements(mode,
      drawVertsUsed,
      GL_UNSIGNED_SHORT,
      lineIndices);

    glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
    glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);
    glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    GL_UseProgram(nullptr);

#if 0
    glBindBuffer(GL_ARRAY_BUFFER, lineVertexBUffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptrARB)(sizeof(idDrawVert) * drawVertsUsed), drawVerts);
  
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, lineIndexBUffer);

    GL_UseProgram(vertexColorProgram);

    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_position, 3, GL_FLOAT, false, sizeof(idDrawVert), attributeOffset(0, idDrawVert::xyzOffset));
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_color, 4, GL_UNSIGNED_BYTE, false, sizeof(idDrawVert), attributeOffset(0, idDrawVert::colorOffset));

    glUniformMatrix4fv(glslProgramDef_t::uniform_modelViewMatrix, 1, false, GL_ModelViewMatrix.Top());
    glUniformMatrix4fv(glslProgramDef_t::uniform_projectionMatrix, 1, false, GL_ProjectionMatrix.Top());

    glDrawElements(GL_LINES,
      drawVertsUsed,
      GL_UNSIGNED_SHORT,
      0);

    glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
    glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    GL_UseProgram(nullptr);
#endif
  } else {

    if(currentTexture)
      currentTexture->Bind();

    glBegin(currentMode);

    for (int i = 0; i < drawVertsUsed; ++i) {
      const idDrawVert& vertex = drawVerts[i];
      glColor4ubv(&vertex.color[0]);
      glTexCoord2fv(vertex.st.ToFloatPtr());
      glVertex3fv(vertex.xyz.ToFloatPtr());
    }

    glEnd();
  }
}

void fhImmediateMode::TexCoord2f(float s, float t)
{
  currentTexCoord[0] = s;
  currentTexCoord[1] = t;
}

void fhImmediateMode::Color3f(float r, float g, float b)
{
  currentColor[0] = static_cast<byte>(r * 255.0f);
  currentColor[1] = static_cast<byte>(g * 255.0f);
  currentColor[2] = static_cast<byte>(b * 255.0f);
  currentColor[3] = static_cast<byte>(1.0f * 255.0f);
}

void fhImmediateMode::Color3fv(const float* c)
{
  currentColor[0] = static_cast<byte>(c[0] * 255.0f);
  currentColor[1] = static_cast<byte>(c[1] * 255.0f);
  currentColor[2] = static_cast<byte>(c[2] * 255.0f);
  currentColor[3] = static_cast<byte>(1.0f * 255.0f);
}

void fhImmediateMode::Vertex3fv(const float* c)
{
  Vertex3f(c[0], c[1], c[2]);
}

void fhImmediateMode::Vertex3f(float x, float y, float z)
{
  if (drawVertsUsed >= drawVertsCapacity)
    return;

  idDrawVert& vertex = drawVerts[drawVertsUsed++];
  vertex.xyz.Set(x, y, z);
  vertex.st.Set(currentTexCoord[0], currentTexCoord[1]);
  vertex.color[0] = currentColor[0];
  vertex.color[1] = currentColor[1];
  vertex.color[2] = currentColor[2];
  vertex.color[3] = currentColor[3];

  if(backEnd.glslEnabled) {
    //we don't want to draw deprecated quads... correct them by re-adding previous
    // vertices, so we render two triangles instead of one quad

    if(currentMode == GL_QUADS && drawVertsUsed >= 4 && (drawVertsUsed % 4) == 0 && drawVertsUsed+2 <= drawVertsCapacity) {
      drawVerts[drawVertsUsed+1] = drawVerts[drawVertsUsed-3];
      drawVerts[drawVertsUsed+2] = drawVerts[drawVertsUsed-1];
      drawVertsUsed += 2;
    }
  }
}

void fhImmediateMode::Vertex2f(float x, float y)
{
  Vertex3f(x, y, 0.0f);
}