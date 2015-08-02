#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"
#include "ImmediateMode.h"

namespace {
  const int drawVertsCapacity = (1 << 14);

  //TODO: we could use a special fhSimpleVertex with just xyz, color and texcoord
  //      this would reduce the memory footprint and buffer updates might be 
  //      faster due to less memory... probably not that important for debug
  //      stuff!?
  idDrawVert drawVerts[drawVertsCapacity];
  unsigned short lineIndices[drawVertsCapacity * 2];

  bool active = false;

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
  assert(!active);
  active = true;
}

fhImmediateMode::~fhImmediateMode()
{
  End();
  assert(active);
  active = false;
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
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_position, 3, GL_FLOAT, false, sizeof(idDrawVert), GL_AttributeOffset(offset, idDrawVert::xyzOffset));
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_color, 4, GL_UNSIGNED_BYTE, false, sizeof(idDrawVert), GL_AttributeOffset(offset, idDrawVert::colorOffset));
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_texcoord, 2, GL_FLOAT, false, sizeof(idDrawVert), GL_AttributeOffset(offset, idDrawVert::texcoordOffset));

    glUniformMatrix4fv(glslProgramDef_t::uniform_modelViewMatrix, 1, false, GL_ModelViewMatrix.Top());
    glUniformMatrix4fv(glslProgramDef_t::uniform_projectionMatrix, 1, false, GL_ProjectionMatrix.Top());
    glUniform4f(glslProgramDef_t::uniform_diffuse_color, 1, 1, 1, 1);

    GLenum mode = currentMode;
    
    if(mode == GL_QUADS || mode == GL_POLYGON) //quads and polygons are replaced by triangles in GLSL mode
      mode = GL_TRIANGLES;

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

  drawVertsUsed = 0;
}

void fhImmediateMode::TexCoord2f(float s, float t)
{
  currentTexCoord[0] = s;
  currentTexCoord[1] = t;
}

void fhImmediateMode::Color4f(float r, float g, float b, float a)
{
  currentColor[0] = static_cast<byte>(r * 255.0f);
  currentColor[1] = static_cast<byte>(g * 255.0f);
  currentColor[2] = static_cast<byte>(b * 255.0f);
  currentColor[3] = static_cast<byte>(a * 255.0f);
}

void fhImmediateMode::Color3f(float r, float g, float b)
{
  Color4f(r, g, b, 1.0f);
}

void fhImmediateMode::Color3fv(const float* c)
{
  Color4f(c[0], c[1], c[2], 1.0f);  
}

void fhImmediateMode::Color4fv(const float* c)
{
  Color4f(c[0], c[1], c[2], c[3]);  
}

void fhImmediateMode::Color4ubv(const byte* bytes)
{
  currentColor[0] = bytes[0];
  currentColor[1] = bytes[1];
  currentColor[2] = bytes[2];
  currentColor[3] = bytes[3];
}

void fhImmediateMode::Vertex3fv(const float* c)
{
  Vertex3f(c[0], c[1], c[2]);
}

void fhImmediateMode::Vertex3f(float x, float y, float z)
{
  if (drawVertsUsed + 1 >= drawVertsCapacity)
    return;

  //we don't want to draw deprecated quads/polygons... correct them by re-adding
  // previous vertices, so we render triangles instead of quads/polygons
  // NOTE: this only works for convex polygons (just as GL_POLYGON)
  if (backEnd.glslEnabled && 
    (currentMode == GL_POLYGON || currentMode == GL_QUADS) &&
    drawVertsUsed >= 3 &&
    drawVertsUsed + 3 < drawVertsCapacity)
  {
    drawVerts[drawVertsUsed] = drawVerts[0];
    drawVerts[drawVertsUsed + 1] = drawVerts[drawVertsUsed - 1];
    drawVertsUsed += 2;
  }

  idDrawVert& vertex = drawVerts[drawVertsUsed++];
  vertex.xyz.Set(x, y, z);
  vertex.st.Set(currentTexCoord[0], currentTexCoord[1]);
  vertex.color[0] = currentColor[0];
  vertex.color[1] = currentColor[1];
  vertex.color[2] = currentColor[2];
  vertex.color[3] = currentColor[3];
}

void fhImmediateMode::Vertex2f(float x, float y)
{
  Vertex3f(x, y, 0.0f);
}