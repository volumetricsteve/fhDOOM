#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "../../renderer/tr_local.h"
#include "rendertools.h"

static const int maxVerticesPerCommit = 1024 * 4 * 3;

static const unsigned short* indices() {
  static unsigned short* p = nullptr;
  if(!p) {
    p = new unsigned short[maxVerticesPerCommit];
    for(int i=0; i<maxVerticesPerCommit; ++i)
      p[i] = i;
  }

  return p;
}

fhTrisBuffer::fhTrisBuffer() {
  vertices.Resize(4096);
}

void fhTrisBuffer::Add(const fhSimpleVert* vertices, int verticesCount) {
  assert(verticesCount % 3 == 0);
  for(int i=0; i<verticesCount; ++i)  
    this->vertices.Append(vertices[i]);
}


void fhTrisBuffer::Add(idVec3 a, idVec3 b, idVec3 c, idVec4 color) {

  fhSimpleVert vert;
  vert.color[0] = static_cast<byte>(color[0] * 255.0f);
  vert.color[1] = static_cast<byte>(color[1] * 255.0f);
  vert.color[2] = static_cast<byte>(color[2] * 255.0f);
  vert.color[3] = static_cast<byte>(color[3] * 255.0f);  
  
  vert.xyz = a;
  vertices.Append(vert);

  vert.xyz = b;
  vertices.Append(vert);

  vert.xyz = c;
  vertices.Append(vert);
}

void fhTrisBuffer::Add(const fhSimpleVert& a, const fhSimpleVert& b, const fhSimpleVert& c) {
  vertices.Append(a);
  vertices.Append(b);
  vertices.Append(c);
}

void fhTrisBuffer::Clear() {
  vertices.SetNum(0, false);
}

const fhSimpleVert* fhTrisBuffer::Vertices() const {
  return vertices.Ptr();
}

int fhTrisBuffer::TriNum() const {
  assert(vertices.Num() % 3 == 0);
  return vertices.Num() / 3;
}

void fhTrisBuffer::Commit(idImage* texture, const idVec4& colorModulate, const idVec4& colorAdd) {
  const int verticesUsed = vertices.Num();

  if (verticesUsed > 0)
  {
    if (texture) {
      GL_SelectTexture(1);
      texture->Bind();
      if (texture->type == TT_CUBIC)
        GL_UseProgram(skyboxProgram);
      else
        GL_UseProgram(defaultProgram);
      GL_SelectTexture(0);
    }
    else {
      GL_UseProgram(vertexColorProgram);
    }

    glUniformMatrix4fv(glslProgramDef_t::uniform_modelViewMatrix, 1, false, GL_ModelViewMatrix.Top());
    glUniformMatrix4fv(glslProgramDef_t::uniform_projectionMatrix, 1, false, GL_ProjectionMatrix.Top());
    glUniform4f(glslProgramDef_t::uniform_diffuse_color, 1, 1, 1, 1);
    glUniform4fv(glslProgramDef_t::uniform_color_add, 1, colorAdd.ToFloatPtr());
    glUniform4fv(glslProgramDef_t::uniform_color_modulate, 1, colorModulate.ToFloatPtr());

    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);
    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);

    int verticesCommitted = 0;
    while (verticesCommitted < verticesUsed)
    {
      int verticesToCommit = min(maxVerticesPerCommit, verticesUsed - verticesCommitted);

      auto vert = vertexCache.AllocFrameTemp(&vertices[verticesCommitted], verticesToCommit * sizeof(fhSimpleVert));
      int offset = vertexCache.Bind(vert);

      glVertexAttribPointer(glslProgramDef_t::vertex_attrib_position, 3, GL_FLOAT, false, sizeof(fhSimpleVert), GL_AttributeOffset(offset, fhSimpleVert::xyzOffset));
      glVertexAttribPointer(glslProgramDef_t::vertex_attrib_color, 4, GL_UNSIGNED_BYTE, false, sizeof(fhSimpleVert), GL_AttributeOffset(offset, (void*)fhSimpleVert::colorOffset));
      glVertexAttribPointer(glslProgramDef_t::vertex_attrib_texcoord, 2, GL_FLOAT, false, sizeof(fhSimpleVert), GL_AttributeOffset(offset, (void*)fhSimpleVert::texcoordOffset));

      glDrawElements(GL_TRIANGLES,
        verticesToCommit,
        GL_UNSIGNED_SHORT,
        indices());

      verticesCommitted += verticesToCommit;
    }

    glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
    glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);
    glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_texcoord);
  }

  Clear();
}


fhSurfaceBuffer::fhSurfaceBuffer() {
}

fhSurfaceBuffer::~fhSurfaceBuffer() {
  for(int i=0; i<entries.Num(); ++i) {
    delete entries[i];
  }
}

fhTrisBuffer* fhSurfaceBuffer::GetMaterialBuffer(const idMaterial* material)
{
  if(!material)
    return GetColorBuffer();

  for(int i=0; i<entries.Num(); ++i)
  {
    if(entries[i]->material == nullptr)  {
      entries[i]->material = material;
      entries[i]->trisBuffer.Clear();
      return &entries[i]->trisBuffer;
    }

    if(entries[i]->material == material)
      return &entries[i]->trisBuffer;
  }

  entry_t* entry = new entry_t();
  entry->material = material;
  entries.Append(entry);

  return &entry->trisBuffer;
}

fhTrisBuffer* fhSurfaceBuffer::GetColorBuffer()
{
  return &coloredTrisBuffer;
}

void fhSurfaceBuffer::Clear() {
  for(int i=0; i<entries.Num(); ++i) {
    const auto& entry = entries[i];
    entry->trisBuffer.Clear();
    entry->material = nullptr;
  }
}

void fhSurfaceBuffer::Commit(const idVec4& colorModulate, const idVec4& colorAdd) {
  for(int i=0; i<entries.Num(); ++i) {
    entry_t* entry = entries[i];

    if(!entry->material) 
      break;

    entries[i]->trisBuffer.Commit(entry->material->GetEditorImage(), colorModulate, colorAdd);
  }

  this->coloredTrisBuffer.Commit(nullptr, colorModulate, colorAdd);
}








fhPointBuffer::fhPointBuffer() {
}
fhPointBuffer::~fhPointBuffer() {
  for(int i=0; i<entries.Num(); ++i) {
    delete entries[i];
  }
}

void fhPointBuffer::Add(const idVec3& xyz, const idVec4& color, float size) {
  if(size <= 0.001f)
    return;

  fhSimpleVert vert;
  vert.xyz = xyz;
  vert.SetColor(color);

  for(int i=0; i<entries.Num(); ++i) {
    entry_t* e = entries[i];
    if(e->size <= 0.0) {
      e->size = size;
      e->vertices.Append(vert);
      return;
    }

    if(abs(size - e->size) < 0.001) {
      e->vertices.Append(vert);
      return;
    }
  }

  entry_t* e = new entry_t();
  e->size = size;
  e->vertices.Append(vert);
  entries.Append(e);
}

void fhPointBuffer::Add(const idVec3& xyz, const idVec3& color, float size) {
  Add(xyz, idVec4(color, 1.0f), size);
}

void fhPointBuffer::Clear() {
  for(int i=0; i<entries.Num(); ++i) {
    entry_t* e = entries[i];
    e->vertices.SetNum(0, false);
    e->size = -1;
  }
}

void fhPointBuffer::Commit() {
  for (int i = 0; i < entries.Num(); ++i) {
    entry_t* e = entries[i];
    
    if(e->size <= 0.0f)
      return;

    e->Commit();
    e->size = -1;
    e->vertices.SetNum(0, false);
  }  
}

void fhPointBuffer::entry_t::Commit() {
  const int verticesUsed = vertices.Num();

  if (verticesUsed > 0)
  {
    glPointSize(size);
    GL_UseProgram(vertexColorProgram);

    glUniformMatrix4fv(glslProgramDef_t::uniform_modelViewMatrix, 1, false, GL_ModelViewMatrix.Top());
    glUniformMatrix4fv(glslProgramDef_t::uniform_projectionMatrix, 1, false, GL_ProjectionMatrix.Top());
    glUniform4f(glslProgramDef_t::uniform_diffuse_color, 1, 1, 1, 1);
    glUniform4f(glslProgramDef_t::uniform_color_add, 0, 0, 0, 0);
    glUniform4f(glslProgramDef_t::uniform_color_modulate, 1, 1, 1, 1);

    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
    glEnableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);

    int verticesCommitted = 0;
    while (verticesCommitted < verticesUsed)
    {
      int verticesToCommit = min(maxVerticesPerCommit, verticesUsed - verticesCommitted);

      auto vert = vertexCache.AllocFrameTemp(&vertices[verticesCommitted], verticesToCommit * sizeof(fhSimpleVert));
      int offset = vertexCache.Bind(vert);

      glVertexAttribPointer(glslProgramDef_t::vertex_attrib_position, 3, GL_FLOAT, false, sizeof(fhSimpleVert), GL_AttributeOffset(offset, fhSimpleVert::xyzOffset));
      glVertexAttribPointer(glslProgramDef_t::vertex_attrib_color, 4, GL_UNSIGNED_BYTE, false, sizeof(fhSimpleVert), GL_AttributeOffset(offset, (void*)fhSimpleVert::colorOffset));

      glDrawElements(GL_POINTS,
        verticesToCommit,
        GL_UNSIGNED_SHORT,
        ::indices());

      verticesCommitted += verticesToCommit;
    }

    glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_position);
    glDisableVertexAttribArray(glslProgramDef_t::vertex_attrib_color);
    glPointSize(1);
  }
}