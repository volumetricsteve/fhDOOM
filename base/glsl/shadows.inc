layout(binding = 6) uniform sampler2D texture6;
layout(binding = 7) uniform sampler2D texture7;
layout(binding = 8) uniform sampler2D texture8;
layout(binding = 9) uniform sampler2D texture9;
layout(binding = 10) uniform sampler2D texture10;
layout(binding = 11) uniform sampler2D texture11;

vec4 offset_lookup(sampler2D map, vec2 coord, vec2 offset)
{
  return texture(map, coord + offset);
}

float isOccluded(sampler2D map, vec2 coord, vec2 offset, float ref)
{
  float f = offset_lookup(map, coord, offset).x;
  if( f < ref )
    return 1.0;
  else
    return 0.0;
}

float getShadow(vec4 pos, sampler2D tex, vec2 coordScale, vec2 coordOffset)
{   
    pos = pos / pos.w;

    pos.x = pos.x/2.0 + 0.5;
    pos.y = pos.y/2.0 + 0.5;
    pos.z = pos.z/2.0 + 0.5;   

    pos.x *= coordScale.x; 
    pos.y *= coordScale.y; 

    pos.x += coordOffset.x;
    pos.y += coordOffset.y;

//#define FILTER_PCF
//#define FILTER_PCF4
//#define FILTER_PCF9
//#define FILTER_PCF13
#define FILTER_PCF21

  //vec2 texsize = 1.0/textureSize(tex, 0) * 3 * rpShadowParams.x;
  vec2 texsize = vec2(1,1) * 0.003 * rpShadowParams.x;

#if defined(FILTER_PCF4)
    const vec2 kernel2x2[4] = vec2[](
      vec2(1,0),
      vec2(-1,0),
      vec2(0,1),
      vec2(0,-1)
    );

    float foo = 0;
    for(int i=0;i<4; ++i) {
       float f = texture(tex, pos.st + kernel2x2[i] * texsize).x;
       if( f <= pos.z )
          foo += 1.0;       
    }

    return foo/4.0;

#elif defined(FILTER_PCF9)
    const vec2 kernel3x3[9] = vec2[](
      vec2(-1,1),
      vec2(0,1),
      vec2(1,1),
      vec2(-1,0),
      vec2(0,0),
      vec2(1,0),
      vec2(-1,-1),
      vec2(0,-1),
      vec2(1,-1)
    );

    float foo = 0;
    for(int i=0;i<9; ++i) {
       float f = texture(tex, pos.st + kernel3x3[i] * texsize).x;
       if( f < pos.z )
          foo += 1.0;       
    }

    return foo/9.0;

#elif defined(FILTER_PCF13)
    const vec2 kernel3x3[13] = vec2[](
      vec2(0, 1),
      vec2(0, -1),
      vec2(-1, 0),
      vec2(1, 0),      
      vec2(-0.5,0.5),
      vec2(0,0.5),
      vec2(0.5,0.5),
      vec2(-0.5,0),
      vec2(0,0),
      vec2(0.5,0),
      vec2(-0.5,-0.5),
      vec2(0,-0.5),
      vec2(0.5,-0.5)
    );

    float foo = 0;
    for(int i=0;i<13; ++i) {
       float f = texture(tex, pos.st + kernel3x3[i] * texsize).x;
       if( f < pos.z )
          foo += 1.0;       
    }

    return foo/13.0;

#elif defined(FILTER_PCF21)
    const vec2 kernel3x3[21] = vec2[](
      vec2(0.5, 1),
      vec2(0.5, -1),
      vec2(-0.5, 1),
      vec2(-0.5, -1),
      vec2(1, 0.5),
      vec2(-1, 0.5),
      vec2(1, -0.5),
      vec2(-1, -0.5),

      vec2(0, 1),
      vec2(0, -1),
      vec2(-1, 0),
      vec2(1, 0),

      vec2(-0.5,0.5),
      vec2(0,0.5),
      vec2(0.5,0.5),
      vec2(-0.5,0),
      vec2(0,0),
      vec2(0.5,0),
      vec2(-0.5,-0.5),
      vec2(0,-0.5),
      vec2(0.5,-0.5)
    );

    int foo = 0;
    int i=0;
    for(;i<8; ++i) {
       float f = texture(tex, pos.st + kernel3x3[i] * texsize).x;
       if( f < pos.z )
          foo += 1;       
    }
/*
    if(foo == 0)
      return 0.0;

    if(foo == 8)
      return 1.0;
*/
    for(;i<21; ++i) {
       float f = texture(tex, pos.st + kernel3x3[i] * texsize).x;
       if( f < pos.z )
          foo += 1;       
    }

    return foo/21.0;

#elif defined(FILTER_PCF)
    float occluded = 0;
    int samplesTaken = 0;
    float d = 0.003 * rpShadowParams.x; //modulate 'base' softness by shadowSoftness parameter

    if(d<0.0001)
      return isOccluded(tex, pos.st, vec2(0,0), pos.z);

    float s = d/4;
    for(float i=-d;i<d;i+=s) {
      for(float j=-s;j<d;j+=s) {

        vec2 coord = pos.st + vec2(i,j);
                    
        
        float f = texture(tex, coord).x;
        if( f < pos.z )
          occluded += 1.0;

        samplesTaken += 1;
      }     
    }   

    return occluded/samplesTaken;
#else    
    return isOccluded(tex, pos.st, vec2(0,0), pos.z);
#endif  

}

float projectedShadow(vec4 pos)
{
  return getShadow(pos, texture6, rpShadowCoords[0].xy, rpShadowCoords[0].zw);
}

float pointlightShadow(vec4 pos[6], vec3 toGlobalLightOrigin)
{  
  vec3 d = toGlobalLightOrigin;

  int side = 0;
  float l = d.x;


  if( d.y > l ) {
    side = 1;
    l = d.y;
  }
  if( d.z > l ) {
    side = 2;
    l = d.z;
  }
  if( -d.x > l ) {
    side = 3;
    l = -d.x;
  }
  if( -d.y > l ) {
    side = 4;
    l = -d.y;
  }
  if( -d.z > l ) {
    side = 5;
    l = -d.z;
  }   

  if(side == 0)
  {  
    return getShadow(pos[0], texture6, rpShadowCoords[0].xy, rpShadowCoords[0].zw); 
  }

  if(side == 1)
  {  
    return getShadow(pos[2], texture8, rpShadowCoords[2].xy, rpShadowCoords[2].zw); 
  }  

  if(side == 2)
  {  
    return getShadow(pos[4], texture10, rpShadowCoords[4].xy, rpShadowCoords[4].zw); 
  }   

  if(side == 3)
  {  
    return getShadow(pos[1], texture7, rpShadowCoords[1].xy, rpShadowCoords[1].zw); 
  }   

  if(side == 4)
  {  
    return getShadow(pos[3], texture9, rpShadowCoords[3].xy, rpShadowCoords[3].zw); 
  }  

  return getShadow(pos[5], texture11, rpShadowCoords[5].xy, rpShadowCoords[5].zw); 
}