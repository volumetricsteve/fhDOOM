/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 2016 Johannes Ohlemacher (http://github.com/eXistence/fhDOOM)

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "global.inc"

layout(binding = 1) uniform sampler2D texture1;

in vs_output
{
  vec4 color;
  vec2 texcoord;
} frag;

out vec4 result;

void main(void)
{ 
  float t = shaderParm0.x;
  vec4 color = texture2D(texture1, frag.texcoord);
  
  float brightness = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
  if(brightness > 0.1)  
    result = vec4(max(vec3(0,0,0), color.rgb - vec3(t,t,t)), 1);  
  else
    result = vec4(0,0,0,1);  

  

  if (color.x > t*1.2)   
    result.x = color.x;

  if (color.y > t*1.2) 
    result.y = color.y;

  if (color.z > t) 
    result.z = color.z;
  
  result.w = 1;

  //result = vec4(1,0,0,1);
  
}