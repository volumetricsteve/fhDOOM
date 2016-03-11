#include "global.inc"
#line 3 __FILE__

layout(binding = 1) uniform sampler2D texture1;
layout(binding = 2) uniform sampler2D texture2;
layout(binding = 3) uniform sampler2D texture3;
layout(binding = 4) uniform sampler2D texture4;
layout(binding = 5) uniform sampler2D texture5;

layout(binding = 6) uniform sampler2D texture6;
layout(binding = 7) uniform sampler2D texture7;
layout(binding = 8) uniform sampler2D texture8;
layout(binding = 9) uniform sampler2D texture9;
layout(binding = 10) uniform sampler2D texture10;
layout(binding = 11) uniform sampler2D texture11;

in vs_output
{
  vec4 color;
  vec2 texNormal;
  vec2 texDiffuse;
  vec2 texSpecular;
  vec4 texLight;
  vec3 L;
  vec3 V;
  vec3 H;
  vec4 shadow[6];
  vec3 toGlobalLightOrigin;  
} frag;

out vec4 result;

// texture 0 is the cube map
// texture 1 is the per-surface bump map
// texture 2 is the light falloff texture
// texture 3 is the light projection texture
// texture 4 is the per-surface diffuse map
// texture 5 is the per-surface specular map
// texture 6 is the specular lookup table


struct TextureData
{
  vec4 diffuse;
  vec4 specular;
  vec4 normal;
  vec3 lightFalloff;
  vec3 lightProjection;
};

vec4 testColor = vec4(1,1,1,1);

vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir)
{ 
    // number of depth layers
    const float minLayers = 8;
    const float maxLayers = 16;
    float height_scale = rpPomMaxHeight;

    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));  
    // calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    vec2 P = viewDir.xy / viewDir.z * height_scale; 
    vec2 deltaTexCoords = P / numLayers;
  
    // get initial values
    vec2  currentTexCoords     = texCoords;
    float currentDepthMapValue = texture(texture5, currentTexCoords).a;
      
    int i = 0;
    while(currentLayerDepth < currentDepthMapValue && i < 40)
    {
        // shift texture coordinates along direction of P
        currentTexCoords += deltaTexCoords;
        // get depthmap value at current texture coordinates
        currentDepthMapValue = texture(texture5, currentTexCoords).a;  
        // get depth of next layer
        currentLayerDepth += layerDepth;  
        i++;
    }    
    
    /*
    if(i >= minLayers)
      testColor = vec4(1,0,0,1);
    if(i >= 6)
      testColor = vec4(0,1,0,1);
    if(i >= 10)
      testColor = vec4(0,0,1,1);    
    */

    // -- parallax occlusion mapping interpolation from here on
    // get texture coordinates before collision (reverse operations)
    vec2 prevTexCoords = currentTexCoords - deltaTexCoords;

    // get depth after and before collision for linear interpolation
    float afterDepth  = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = texture(texture5, prevTexCoords).a - currentLayerDepth + layerDepth;
 
    // interpolation of texture coordinates
    float weight = afterDepth / (afterDepth - beforeDepth);
    vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

    return finalTexCoords;
}

TextureData GetTextureData(vec3 viewDir)
{
  TextureData data;

  vec4 tmp = texture(texture5, frag.texSpecular);
  
  //FIXME(johl): add uniform to test if specular map contains height in alpha channel (tmp.a < 0.99 is not sufficient)
  if(rpPomMaxHeight > 0.0 && tmp.a < 0.99)
  {
    //FIXME(johl): pass tmp.a to ParallaxMapping as initial height, saves one texture lookup in ParallaxMapping
    vec2 texcoord = ParallaxMapping(frag.texSpecular.st, viewDir);
     
    //FIXME(johl): if using POM, diffuse, specular and normal map cannot have different texture 
    //             coordinates/matrices
    data.specular = texture(texture5, texcoord);
    data.diffuse = texture(texture4, texcoord) * testColor;
    data.normal = texture(texture1, texcoord);
  }
  else
  {
    data.specular = vec4(tmp.rgb, 1);
    data.diffuse = texture(texture4, frag.texDiffuse);
    data.normal = texture(texture1, frag.texNormal);
  }

  data.lightProjection = texture2DProj(texture3, frag.texLight.xyw).rgb;
  data.lightFalloff = texture2D(texture2, vec2(frag.texLight.z, 0.5)).rgb;
    
  data.specular *= rpSpecularScale;

  return data;
}

vec4 blinnPhong(TextureData textureData, vec3 N, vec3 L)
{
  vec3 H = normalize(frag.H);

  float NdotL = clamp(dot(N, L), 0.0, 1.0);
  float NdotH = clamp(dot(N, H), 0.0, 1.0);

  vec3 diffuseColor = textureData.diffuse.rgb * rpDiffuseColor.rgb;
  vec3 specularColor = 2.0 * textureData.specular.rgb * rpSpecularColor.rgb;

  float specularFalloff = pow(NdotH, rpSpecularExp);

  vec3 color;
  color = diffuseColor;
  color += specularFalloff * specularColor;
  color *= NdotL * textureData.lightProjection;
  color *= textureData.lightFalloff;

  return vec4(color, 1.0) * frag.color;
}

vec4 phong(TextureData textureData, vec3 N, vec3 L, vec3 viewDir)
{
  float NdotL = clamp(dot(N, L), 0.0, 1.0);

  vec3 diffuseColor = textureData.diffuse.rgb * rpDiffuseColor.rgb;
  vec3 specularColor = 2.0 * textureData.specular.rgb * rpSpecularColor.rgb;


  vec3 R = -reflect(L, N);
  float RdotV = clamp(dot(R, viewDir), 0.0, 1.0);
  float specularFalloff = pow(RdotV, rpSpecularExp);

  vec3 color;
  color = diffuseColor;
  color += specularFalloff * specularColor;
  color *= NdotL * textureData.lightProjection;
  color *= textureData.lightFalloff;

  return vec4(color, 1.0) * frag.color;
}


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

float getShadow(vec4 pos, sampler2D tex)
{   
    pos = pos / pos.w;

    pos.x = pos.x/2.0 + 0.5;
    pos.y = pos.y/2.0 + 0.5;
    pos.z = pos.z/2.0 + 0.5;    

//#define FILTER_PCF
//#define FILTER_PCF4
//#define FILTER_PCF9
//#define FILTER_PCF13
#define FILTER_PCF21

vec2 texsize = 1.0/textureSize(tex, 0) * 3;

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

    if(foo == 0)
      return 0.0;

    if(foo == 8)
      return 1.0;

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

    if(samplesTaken > 40)
      return -100;

    return occluded/samplesTaken;
#else    
    return isOccluded(tex, pos.st, vec2(0,0), pos.z);
#endif  

}

float projectedShadow()
{
  return getShadow(frag.shadow[0], texture6);
}

float pointlightShadow()
{  
  vec3 d = frag.toGlobalLightOrigin;

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
    return getShadow(frag.shadow[0], texture6); 
  }

  if(side == 1)
  {  
    return getShadow(frag.shadow[2], texture8); 
  }  

  if(side == 2)
  {  
    return getShadow(frag.shadow[4], texture10); 
  }   

  if(side == 3)
  {  
    return getShadow(frag.shadow[1], texture7); 
  }   

  if(side == 4)
  {  
    return getShadow(frag.shadow[3], texture9); 
  }  

  if(side == 5)
  {  
    return getShadow(frag.shadow[5], texture11); 
  }   

  return 0;
}

void main(void)
{  
  vec3 viewDir = normalize(frag.V);
  TextureData textureData = GetTextureData(viewDir);    
  vec3 lightDir = normalize(frag.L);
  vec3 normalDir = normalize(2.0 * textureData.normal.agb - 1.0);

  if(rpShading == 1)
    result = phong(textureData, normalDir, lightDir, viewDir);
  else
    result = blinnPhong(textureData, normalDir, lightDir);


  float shadowness = 0;

  if(rpShadowMappingMode == 1)  
    shadowness = pointlightShadow();  
  else if(rpShadowMappingMode == 2)
    shadowness = projectedShadow(); 

  result *= mix(rpShadowParams.y, 1, 1-shadowness);
}
