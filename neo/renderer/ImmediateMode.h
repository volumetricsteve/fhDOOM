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
