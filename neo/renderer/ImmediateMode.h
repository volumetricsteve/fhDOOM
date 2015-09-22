#pragma once

class idDrawVert;

struct fhSimpleVert {
  idVec3 xyz;
  idVec2 st;
  byte color[4];

  static const int xyzOffset = 0;
  static const int texcoordOffset = 12;
  static const int colorOffset = 20;
};

static_assert(sizeof(fhSimpleVert) == 24, "unexpected size of simple vertex, due to padding?");


class fhImmediateMode
{
public:
  explicit fhImmediateMode(bool geometryOnly = false);
  ~fhImmediateMode();

  void SetTexture(idImage* texture);

  void Begin(GLenum mode);
  void TexCoord2f(float s, float t);
  void TexCoord2fv(const float* v);
  void Color3fv(const float* c);  
  void Color3f(float r, float g, float b);  
  void Color4f(float r, float g, float b, float a);
  void Color4fv(const float* c);
  void Color4ubv(const byte* bytes);
  void Vertex3fv(const float* c);
  void Vertex3f(float x, float y, float z);
  void Vertex2f(float x, float y);
  void End();

  void Sphere(float radius, int rings, int sectors, bool inverse = false);

  GLenum getCurrentMode() const { return currentMode; }

  static void AddTrianglesFromPolygon(fhImmediateMode& im, const idVec3* xyz, int num);

  static void Init();
  static void ResetStats();
  static int DrawCallCount();
  static int DrawCallVertexSize();
private:  
  bool geometryOnly;
  float currentTexCoord[2];
  GLenum currentMode;
  byte currentColor[4];
  idImage* currentTexture;

  int drawVertsUsed;

  static int drawCallCount;
  static int drawCallVertexSize;
};

class fhLineBuffer
{
public:
  fhLineBuffer();
  ~fhLineBuffer();

  void Add(idVec3 from, idVec3 to, idVec4 color);
  void Add(idVec3 from, idVec3 to, idVec3 color);
  void Clear();
  void Commit();

private:  
  int verticesAllocated;
  int verticesUsed;
  fhSimpleVert* vertices;
};
