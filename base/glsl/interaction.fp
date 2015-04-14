#include "global.inc"

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
} frag;

out vec4 result;

// texture 0 is the cube map
// texture 1 is the per-surface bump map
// texture 2 is the light falloff texture
// texture 3 is the light projection texture
// texture 4 is the per-surface diffuse map
// texture 5 is the per-surface specular map
// texture 6 is the specular lookup table

vec4 blinnPhong(float specularExponent)
{
  vec3 L = normalize(frag.L);

  vec3 H = normalize(frag.H);
  vec3 N = 2.0 * texture2D(texture1, frag.texNormal.st).agb - 1.0;

  float NdotL = clamp(dot(N, L), 0.0, 1.0);
  float NdotH = clamp(dot(N, H), 0.0, 1.0);

  vec3 lightProjection = texture2DProj(texture3, frag.texLight.xyw).rgb;
  vec3 lightFalloff = texture2D(texture2, vec2(frag.texLight.z, 0.5)).rgb;
  vec3 diffuseColor = texture2D(texture4, frag.texDiffuse).rgb * rpDiffuseColor.rgb;
  vec3 specularColor = 2.0 * texture2D(texture5, frag.texSpecular).rgb * rpSpecularColor.rgb;

  float specularFalloff = pow(NdotH, specularExponent);

  vec3 color;
  color = diffuseColor;
  color += specularFalloff * specularColor;
  color *= NdotL * lightProjection;
  color *= lightFalloff;

  return vec4(color, 1.0) * frag.color;
}


vec4 phong(float specularExponent)
{
  vec3 L = normalize(frag.L);

  vec3 V = normalize(frag.V);
  vec3 N = normalize(2.0 * texture2D(texture1, frag.texNormal.st).agb - 1.0);

  float NdotL = clamp(dot(N, L), 0.0, 1.0);

  vec3 lightProjection = texture2DProj(texture3, frag.texLight.xyw).rgb;
  vec3 lightFalloff = texture2D(texture2, vec2(frag.texLight.z, 0.5)).rgb;
  vec3 diffuseColor = texture2D(texture4, frag.texDiffuse).rgb * rpDiffuseColor.rgb;
  vec3 specularColor = 2.0 * texture2D(texture5, frag.texSpecular).rgb * rpSpecularColor.rgb;


  vec3 R = -reflect(L, N);
  float RdotV = clamp(dot(R, V), 0.0, 1.0);
  float specularFalloff = pow(RdotV, specularExponent);

  vec3 color;
  color = diffuseColor;
  color += specularFalloff * specularColor;
  color *= NdotL * lightProjection;
  color *= lightFalloff;

  return vec4(color, 1.0) * frag.color;
}


void main(void)
{  
  bool usePhong = false;
  float specularExponent = 10.0;

  if(usePhong)
    result = phong(specularExponent);
  else
    result = blinnPhong(specularExponent);
}