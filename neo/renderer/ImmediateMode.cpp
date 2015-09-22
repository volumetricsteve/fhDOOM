#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"
#include "ImmediateMode.h"

namespace {
  const int drawVertsCapacity = (1 << 14);

  fhSimpleVert drawVerts[drawVertsCapacity];
  unsigned short lineIndices[drawVertsCapacity * 2];
  unsigned short sphereIndices[drawVertsCapacity * 2];

  bool active = false;

//  GLuint lineVertexBUffer;
//  GLuint lineIndexBUffer;


}

int fhImmediateMode::drawCallCount = 0;
int fhImmediateMode::drawCallVertexSize = 0;

void fhImmediateMode::Init()
{
  for(int i=0; i<drawVertsCapacity*2; ++i)
  {
    lineIndices[i] = i;
  }

  ResetStats();
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

void fhImmediateMode::ResetStats()
{
  drawCallCount = 0;
  drawCallVertexSize = 0;
}
int fhImmediateMode::DrawCallCount()
{
  return drawCallCount;
}

int fhImmediateMode::DrawCallVertexSize()
{
  return drawCallVertexSize;
}


fhImmediateMode::fhImmediateMode(bool geometryOnly)
: drawVertsUsed(0)
, currentTexture(nullptr)
, geometryOnly(geometryOnly)
, currentMode(GL_INVALID_ENUM)
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
  assert(!active);
  active = true;

  currentMode = mode;
  drawVertsUsed = 0;
}

void fhImmediateMode::End()
{
  active = false;
  if(!drawVertsUsed)
    return;

  if(r_glslEnabled.GetBool() || r_glCoreProfile.GetBool())
  {
    auto vert = vertexCache.AllocFrameTemp(drawVerts, drawVertsUsed * sizeof(fhSimpleVert));
    drawCallVertexSize += drawVertsUsed * sizeof(fhSimpleVert);
    int offset = vertexCache.Bind(vert);

    if(!geometryOnly) {
      if(currentTexture) {
        GL_SelectTexture(1);
        currentTexture->Bind();
        if(currentTexture->type == TT_CUBIC)
          GL_UseProgram(skyboxProgram);
        else
          GL_UseProgram(defaultProgram);
        GL_SelectTexture(0);
      } else {
        GL_UseProgram(vertexColorProgram);
      }

      glUniformMatrix4fv(glslProgramDef_t::uniform_modelViewMatrix, 1, false, GL_ModelViewMatrix.Top());
      glUniformMatrix4fv(glslProgramDef_t::uniform_projectionMatrix, 1, false, GL_ProjectionMatrix.Top());
      glUniform4f(glslProgramDef_t::uniform_diffuse_color, 1, 1, 1, 1);
      glUniform4f(glslProgramDef_t::uniform_color_add, 0,0,0,0);
      glUniform4f(glslProgramDef_t::uniform_color_modulate, 1, 1, 1, 1);
    }

    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);
    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_position, 3, GL_FLOAT, false, sizeof(fhSimpleVert), GL_AttributeOffset(offset, fhSimpleVert::xyzOffset));
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_color, 4, GL_UNSIGNED_BYTE, false, sizeof(fhSimpleVert), GL_AttributeOffset(offset, (void*)fhSimpleVert::colorOffset));
    glVertexAttribPointer(glslProgramDef_t::vertex_attrib_texcoord, 2, GL_FLOAT, false, sizeof(fhSimpleVert), GL_AttributeOffset(offset, (void*)fhSimpleVert::texcoordOffset));

    GLenum mode = currentMode;
    
    if(mode == GL_QUADS || mode == GL_POLYGON || mode == GL_QUAD_STRIP) //quads and polygons are replaced by triangles in GLSL mode
      mode = GL_TRIANGLES;

    glDrawElements(mode,
      drawVertsUsed,
      GL_UNSIGNED_SHORT,
      lineIndices);

    drawCallCount++;    

    glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
    glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);
    glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);

    if(!geometryOnly) {    
      GL_UseProgram(nullptr);
    }
  } else {

    if(currentTexture)
      currentTexture->Bind();

    glBegin(currentMode);

    for (int i = 0; i < drawVertsUsed; ++i) {
      const fhSimpleVert& vertex = drawVerts[i];
      glColor4ubv(&vertex.color[0]);
      glTexCoord2fv(vertex.st.ToFloatPtr());
      glVertex3fv(vertex.xyz.ToFloatPtr());
    }

    glEnd();
  }

  drawVertsUsed = 0;
  currentMode = GL_INVALID_ENUM;
}

void fhImmediateMode::TexCoord2f(float s, float t)
{
  currentTexCoord[0] = s;
  currentTexCoord[1] = t;
}

void fhImmediateMode::TexCoord2fv(const float* v)
{
  TexCoord2f(v[0], v[1]);
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
  if (backEnd.glslEnabled)
  {
    if( (currentMode == GL_POLYGON || currentMode == GL_QUADS) &&
      drawVertsUsed >= 3 &&
      drawVertsUsed + 3 < drawVertsCapacity)
    {
      drawVerts[drawVertsUsed] = drawVerts[0];
      drawVerts[drawVertsUsed + 1] = drawVerts[drawVertsUsed - 1];
      drawVertsUsed += 2;
    }

    if ( currentMode == GL_QUAD_STRIP && drawVertsUsed >= 3 && drawVertsUsed+3 < drawVertsCapacity)
    { 
      if(drawVertsUsed % 6 == 0) {
        drawVerts[drawVertsUsed] = drawVerts[drawVertsUsed - 3];
        drawVerts[drawVertsUsed + 1] = drawVerts[drawVertsUsed - 1];
      } else if(drawVertsUsed % 3 == 0) {
        drawVerts[drawVertsUsed] = drawVerts[drawVertsUsed-1];
        drawVerts[drawVertsUsed+1] = drawVerts[drawVertsUsed-2];        
      }
      drawVertsUsed += 2;
    }
  }

  fhSimpleVert& vertex = drawVerts[drawVertsUsed++];
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



void fhImmediateMode::Sphere(float radius, int rings, int sectors, bool inverse)
{
  assert(!active);
  assert(radius > 0.0f);
  assert(rings > 1);
  assert(sectors > 1);

  float const R = 1. / (float)(rings - 1);
  float const S = 1. / (float)(sectors - 1);  

  int vertexNum = 0;   
  for (int r = 0; r < rings; r++) {
    for (int s = 0; s < sectors; s++) {
      float const y = sin(-(idMath::PI/2.0f) + idMath::PI * r * R);
      float const x = cos(2 * idMath::PI * s * S) * sin(idMath::PI * r * R);
      float const z = sin(2 * idMath::PI * s * S) * sin(idMath::PI * r * R);

      drawVerts[vertexNum].xyz.x = x * radius;
      drawVerts[vertexNum].xyz.y = y * radius;
      drawVerts[vertexNum].xyz.z = z * radius;
      drawVerts[vertexNum].st.x = s*S;
      drawVerts[vertexNum].st.y = r*R;
      drawVerts[vertexNum].color[0] = currentColor[0];
      drawVerts[vertexNum].color[1] = currentColor[1];
      drawVerts[vertexNum].color[2] = currentColor[2];
      drawVerts[vertexNum].color[3] = currentColor[3];

      vertexNum += 1;
    }
  }

  int indexNum = 0;
  for (int r = 0; r < rings - 1; r++) {
    for (int s = 0; s < sectors - 1; s++) {
      if(r == 0) {
        //faces of first ring are single triangles
        sphereIndices[indexNum + 2] = r * sectors + s;        
        sphereIndices[indexNum + 1] = (r + 1) * sectors + s;
        sphereIndices[indexNum + 0] = (r + 1) * sectors + (s + 1);

        indexNum += 3;
      } else if (r == rings - 2) {
        //faces of last ring are single triangles
        sphereIndices[indexNum + 0] = r * sectors + s;
        sphereIndices[indexNum + 1] = r * sectors + (s + 1);
        sphereIndices[indexNum + 2] = (r + 1) * sectors + (s + 1);

        indexNum += 3;
      } else {
        //faces of remaining rings are quads (two triangles)
        sphereIndices[indexNum + 0] = r * sectors + s;
        sphereIndices[indexNum + 1] = r * sectors + (s + 1);
        sphereIndices[indexNum + 2] = (r + 1) * sectors + (s + 1);

        sphereIndices[indexNum + 3] = sphereIndices[indexNum + 2];
        sphereIndices[indexNum + 4] = (r + 1) * sectors + s;
        sphereIndices[indexNum + 5] = sphereIndices[indexNum + 0];

        indexNum += 6;
      }
    }
  } 

  if(inverse) {
    for(int i = 0; i+2 < indexNum; i += 3) {
      unsigned short tmp = sphereIndices[i];
      sphereIndices[i] = sphereIndices[i+2];
      sphereIndices[i+2] = tmp;
    }
  }
  
  GL_UseProgram(vertexColorProgram);  

  auto vert = vertexCache.AllocFrameTemp(drawVerts, vertexNum * sizeof(fhSimpleVert));
  int offset = vertexCache.Bind(vert);

  glUniformMatrix4fv(glslProgramDef_t::uniform_modelViewMatrix, 1, false, GL_ModelViewMatrix.Top());
  glUniformMatrix4fv(glslProgramDef_t::uniform_projectionMatrix, 1, false, GL_ProjectionMatrix.Top());
  glUniform4f(glslProgramDef_t::uniform_diffuse_color, 1, 1, 1, 1);  

  glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
  glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);
  glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);
  glVertexAttribPointer(glslProgramDef_t::vertex_attrib_position, 3, GL_FLOAT, false, sizeof(fhSimpleVert), GL_AttributeOffset(offset, fhSimpleVert::xyzOffset));
  glVertexAttribPointer(glslProgramDef_t::vertex_attrib_color, 4, GL_UNSIGNED_BYTE, false, sizeof(fhSimpleVert), GL_AttributeOffset(offset, (void*)fhSimpleVert::colorOffset));
  glVertexAttribPointer(glslProgramDef_t::vertex_attrib_texcoord, 2, GL_FLOAT, false, sizeof(fhSimpleVert), GL_AttributeOffset(offset, (void*)fhSimpleVert::texcoordOffset));

  glDrawElements(GL_TRIANGLES,
    indexNum,
    GL_UNSIGNED_SHORT,
    sphereIndices);

  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);
  glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);
  
  GL_UseProgram(nullptr);
}

void fhImmediateMode::AddTrianglesFromPolygon(fhImmediateMode& im, const idVec3* xyz, int num)
{
  assert(im.getCurrentMode() == GL_TRIANGLES);

  if(num < 3)
    return;

  for(int i=0; i<num; ++i) {
    if(i>0 && i%3 == 0) {
      im.Vertex3fv(xyz[0].ToFloatPtr());
      im.Vertex3fv(xyz[i-1].ToFloatPtr());
    }
    im.Vertex3fv(xyz[i].ToFloatPtr());    
  }
}



fhLineBuffer::fhLineBuffer()
: verticesUsed(0)
, verticesAllocated(4096)
{
  vertices = new fhSimpleVert[verticesAllocated];
}

fhLineBuffer::~fhLineBuffer()
{
  delete[] vertices;
}

void fhLineBuffer::Add(idVec3 from, idVec3 to, idVec3 color)
{
  Add(from, to, idVec4(color.x, color.y, color.z, 1.0f));
}

void fhLineBuffer::Add(idVec3 from, idVec3 to, idVec4 color)
{
  if(verticesAllocated - verticesUsed < 2)
  {
    fhSimpleVert* n = new fhSimpleVert[verticesUsed * 2];
    memcpy(n, vertices, sizeof(vertices[0]) * verticesUsed);
    delete[] vertices;
    vertices = n;
    verticesAllocated = verticesUsed * 2;
  }

  byte colorBytes[4];
  colorBytes[0] = static_cast<byte>(color.x * 255.0f);
  colorBytes[1] = static_cast<byte>(color.y * 255.0f);
  colorBytes[2] = static_cast<byte>(color.z * 255.0f);
  colorBytes[3] = static_cast<byte>(color.w * 255.0f);

  fhSimpleVert& fromVert = vertices[verticesUsed + 0];
  fhSimpleVert& toVert = vertices[verticesUsed + 1];

  memcpy(fromVert.color, colorBytes, sizeof(colorBytes));
  memcpy(toVert.color, colorBytes, sizeof(colorBytes));
  fromVert.xyz = from;
  toVert.xyz = to;

  verticesUsed += 2;
}

void fhLineBuffer::Commit()
{
  if(verticesUsed > 0)
  {
    GL_UseProgram(vertexColorProgram);

    glUniformMatrix4fv(glslProgramDef_t::uniform_modelViewMatrix, 1, false, GL_ModelViewMatrix.Top());
    glUniformMatrix4fv(glslProgramDef_t::uniform_projectionMatrix, 1, false, GL_ProjectionMatrix.Top());
    glUniform4f(glslProgramDef_t::uniform_diffuse_color, 1, 1, 1, 1);
    glUniform4f(glslProgramDef_t::uniform_color_add, 0, 0, 0, 0);
    glUniform4f(glslProgramDef_t::uniform_color_modulate, 1, 1, 1, 1);

    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);
    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);

    int verticesCommitted = 0;
    while(verticesCommitted < verticesUsed)
    {
      static const int maxVerticesPerCommit = (sizeof(lineIndices)/sizeof(lineIndices[0]))/2;

      int verticesToCommit = min(maxVerticesPerCommit, verticesUsed - verticesCommitted);

      auto vert = vertexCache.AllocFrameTemp(&vertices[verticesCommitted], verticesToCommit * sizeof(fhSimpleVert));
      int offset = vertexCache.Bind(vert);

      glVertexAttribPointer(glslProgramDef_t::vertex_attrib_position, 3, GL_FLOAT, false, sizeof(fhSimpleVert), GL_AttributeOffset(offset, fhSimpleVert::xyzOffset));
      glVertexAttribPointer(glslProgramDef_t::vertex_attrib_color, 4, GL_UNSIGNED_BYTE, false, sizeof(fhSimpleVert), GL_AttributeOffset(offset, (void*)fhSimpleVert::colorOffset));
      glVertexAttribPointer(glslProgramDef_t::vertex_attrib_texcoord, 2, GL_FLOAT, false, sizeof(fhSimpleVert), GL_AttributeOffset(offset, (void*)fhSimpleVert::texcoordOffset));

      glDrawElements(GL_LINES,
        verticesToCommit,
        GL_UNSIGNED_SHORT,
        lineIndices);

      verticesCommitted += verticesToCommit;
    }


    glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
    glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);
    glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);
  }

  verticesUsed = 0;
}

void fhLineBuffer::Clear()
{
  verticesUsed = 0;
}