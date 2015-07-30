#pragma once

class idDrawVert;

class fhLinesMode
{
public:
  fhLinesMode();
  ~fhLinesMode();

  void Begin();
  void Color3fv(const float* c);
  void Vertex3fv(const float* c);
  void End();

  static void Init();
private:  
  GLenum currentMode;
  byte currentColor[4];

  static const int drawVertsCapacity = 1024;
  static idDrawVert drawVerts[drawVertsCapacity];    
  int drawVertsUsed;
};
