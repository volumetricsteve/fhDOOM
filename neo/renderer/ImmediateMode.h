#pragma once

class idDrawVert;

class fhImmediateMode
{
public:
  fhImmediateMode();
  ~fhImmediateMode();

  void SetTexture(idImage* texture);

  void Begin(GLenum mode);
  void TexCoord2f(float s, float t);
  void Color3fv(const float* c);  
  void Color3f(float r, float g, float b);  
  void Color4f(float r, float g, float b, float a);
  void Color4fv(const float* c);
  void Color4ubv(const byte* bytes);
  void Vertex3fv(const float* c);
  void Vertex3f(float x, float y, float z);
  void Vertex2f(float x, float y);
  void End();

  static void Init();
private:  
  float currentTexCoord[2];
  GLenum currentMode;
  byte currentColor[4];
  idImage* currentTexture;

  int drawVertsUsed;
};
