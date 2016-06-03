layout(binding = 6) uniform sampler2D texture6;

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

float linearDepth(float expDepth, float n, float f) {
  float z = (2 * n) / (f + n - expDepth * (f - n));

  return z;
}

float getShadow(vec4 pos, vec2 coordScale, vec2 coordOffset, float softness)
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
  vec2 texsize = coordScale * softness;

#if defined(FILTER_PCF4)
    const vec2 kernel2x2[4] = vec2[](
      vec2(1,0),
      vec2(-1,0),
      vec2(0,1),
      vec2(0,-1)
    );

    float foo = 0;
    for(int i=0;i<4; ++i) {
       float f = texture(texture6, pos.st + kernel2x2[i] * texsize).x;
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
       float f = texture(texture6, pos.st + kernel3x3[i] * texsize).x;
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
       float f = texture(texture6, pos.st + kernel3x3[i] * texsize).x;
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
       float f = texture(texture6, pos.st + kernel3x3[i] * texsize).x;
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
       float f = texture(texture6, pos.st + kernel3x3[i] * texsize).x;
       if( f < pos.z )
          foo += 1;       
    }

    return foo/21.0;

#elif defined(FILTER_PCF)
    float occluded = 0;
    int samplesTaken = 0;
    float d = 0.003 * rpShadowParams.x; //modulate 'base' softness by shadowSoftness parameter

    if(d<0.0001)
      return isOccluded(texture6, pos.st, vec2(0,0), pos.z);

    float s = d/4;
    for(float i=-d;i<d;i+=s) {
      for(float j=-s;j<d;j+=s) {

        vec2 coord = pos.st + vec2(i,j);
                    
        
        float f = texture(texture6, coord).x;
        if( f < pos.z )
          occluded += 1.0;

        samplesTaken += 1;
      }     
    }   

    return occluded/samplesTaken;
#else    
    return isOccluded(texture6, pos.st, vec2(0,0), pos.z);
#endif  

}

vec4 projectedShadow(vec4 pos, float softness)
{
  float f = getShadow(pos, rpShadowCoords[0].xy, rpShadowCoords[0].zw, softness);

  f = mix(1, rpShadowParams.y, f);

  return vec4(f,f,f,f);
}

vec4 getParallelShadow(vec4 pos[6], int index, float softness)
{
  vec4 debugColors[6] = vec4[](
    vec4(1,1,1,1),
    vec4(1,0,0,1),
    vec4(1,1,0,1),
    vec4(0,1,0,1),
    vec4(0,1,1,1),
    vec4(0,0,1,1)
  );

  float f = getShadow(pos[index], rpShadowCoords[index].xy, rpShadowCoords[index].zw, softness);

  float ret = mix(1, rpShadowParams.y, f);

#if 0
  return debugColors[index] * ret;
#else
  return vec4(ret, ret, ret, 1);
#endif    
}

vec4 parallelShadow(vec4 pos[6], float distance, float softness)
{  
  //FIXME(johl): this is just a hack to compensate for increasing softness with distance from viewer.
  //             Sooner or later, i need to completely rewrite the way softness for (all) shadows is 
  //             handled and calculated.
  float softscale[6] = float[](
    0.8,
    0.45,
    0.3,
    0.1,
    0.1,
    0.1
  );

  for(int i=0; i<5; ++i)
  {
    if(distance < rpCascadeDistances[i])
      return getParallelShadow(pos, i, softness * softscale[i]);
  }

  return getParallelShadow(pos, 5, softness * softscale[5]);  
}

vec4 pointlightShadow(vec4 pos[6], vec3 toGlobalLightOrigin, float softness)
{  
  vec3 d = toGlobalLightOrigin;

  int side = 0;
  float l = d.x;


  if( d.y > l ) {
    side = 2;
    l = d.y;
  }
  if( d.z > l ) {
    side = 4;
    l = d.z;
  }
  if( -d.x > l ) {
    side = 1;
    l = -d.x;
  }
  if( -d.y > l ) {
    side = 3;
    l = -d.y;
  }
  if( -d.z > l ) {
    side = 5;
    l = -d.z;
  }   

  float f = getShadow(pos[side], rpShadowCoords[side].xy, rpShadowCoords[side].zw, softness);  

  f = mix(1, rpShadowParams.y, f);

  return vec4(f,f,f,f);   
}