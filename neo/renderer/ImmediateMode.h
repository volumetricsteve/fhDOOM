#pragma once

class idDrawVert;

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

  static void Init();
private:  
  bool geometryOnly;
  float currentTexCoord[2];
  GLenum currentMode;
  byte currentColor[4];
  idImage* currentTexture;

  int drawVertsUsed;
};

