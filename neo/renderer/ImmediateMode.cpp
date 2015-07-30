#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"
#include "ImmediateMode.h"

idDrawVert fhLinesMode::drawVerts[fhLinesMode::drawVertsCapacity];  

static GLuint lineVertexBUffer;
static GLuint lineIndexBUffer;

void fhLinesMode::Init()
{
  auto linesIndices  = new unsigned short[drawVertsCapacity * 2];
  for(int i=0; i<drawVertsCapacity*2; ++i)
  {
    linesIndices[i] = i;
  }

  const size_t vertexSize = sizeof(drawVerts);
  const size_t indexSize = drawVertsCapacity * 2 * sizeof(linesIndices[0]);

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
}


fhLinesMode::fhLinesMode()
: drawVertsUsed(0)
{
}

fhLinesMode::~fhLinesMode()
{
  End();
}

void fhLinesMode::Begin()
{
  End();
  drawVertsUsed = 0;
}


template<typename T>
static const void* attributeOffset(T offset, const void* attributeOffset)
{
  return reinterpret_cast<const void*>((std::ptrdiff_t)offset + (std::ptrdiff_t)attributeOffset);
}

void fhLinesMode::End()
{
  if(!drawVertsUsed)
    return;

  if(backEnd.glslEnabled)
  {
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
  } else {
    glBegin(GL_LINES);

    for (int i = 0; i < drawVertsUsed; ++i) {
      const idDrawVert& vertex = drawVerts[i];
      glColor4ubv(&vertex.color[0]);
      glVertex3fv(vertex.xyz.ToFloatPtr());
    }

    glEnd();
  }
}

void fhLinesMode::Color3fv(const float* c)
{
  currentColor[0] = static_cast<byte>(c[0] * 255.0f);
  currentColor[1] = static_cast<byte>(c[1] * 255.0f);
  currentColor[2] = static_cast<byte>(c[2] * 255.0f);
  currentColor[3] = static_cast<byte>(1.0f * 255.0f);
}

void fhLinesMode::Vertex3fv(const float* c)
{
  if(drawVertsUsed >= drawVertsCapacity)
    return;

  idDrawVert& vertex = drawVerts[drawVertsUsed++];
  vertex.xyz.Set(c[0], c[1], c[2]);
  vertex.color[0] = currentColor[0];
  vertex.color[1] = currentColor[1];
  vertex.color[2] = currentColor[2];
  vertex.color[3] = currentColor[3];  
}