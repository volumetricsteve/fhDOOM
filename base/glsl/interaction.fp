#include "global.inc"
#include "shadows.inc"
#include "shading.inc"

layout(binding = 1) uniform sampler2D normalMap;
layout(binding = 2) uniform sampler2D lightFalloff;
layout(binding = 3) uniform sampler2D lightTexture;
layout(binding = 4) uniform sampler2D diffuseMap;
layout(binding = 5) uniform sampler2D specularMap;

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

vec4 diffuse(vec3 N) 
{
  return texture(diffuseMap, frag.texDiffuse) * rpDiffuseColor * lambert(N, frag.L);
}

vec4 specular(vec3 N)
{
  vec4 spec = texture(specularMap, frag.texSpecular) * rpSpecularColor * rpSpecularScale;
  if(rpShading == 1) {
    spec *= phong(N, frag.L, frag.V, rpSpecularExp);
  } else {
    frag.H = normalize(frag.H);
    spec *= blinn(N, frag.H, rpSpecularExp);
  }

  return spec;
}

float shadow()
{
  float shadowness = 0;

  if(rpShadowMappingMode == 1)  
    shadowness = pointlightShadow(frag.shadow, frag.toGlobalLightOrigin);  
  else if(rpShadowMappingMode == 2)
    shadowness = projectedShadow(frag.shadow[0]); 

  return mix(1, rpShadowParams.y, shadowness);  
}

void main(void)
{  
  frag.V = normalize(frag.V);
  frag.L = normalize(frag.L);  

  if(rpPomMaxHeight > 0.0)
  {
    vec2 offset = parallaxOffset(specularMap, frag.texSpecular.st, frag.V);
     
    frag.texSpecular += offset;
    frag.texDiffuse += offset;
    frag.texNormal += offset;    
  }    
  
  vec3 N = normalize(2.0 * texture(normalMap, frag.texNormal).agb - 1.0);

  result = vec4(0,0,0,0);

  result += diffuse(N);
  result += specular(N);

  result *= frag.color;
  result *= texture2DProj(lightTexture, frag.texLight.xyw);
  result *= texture2D(lightFalloff, vec2(frag.texLight.z, 0.5));
  result *= shadow();
}
