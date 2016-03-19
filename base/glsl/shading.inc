float lambert(vec3 N, vec3 L)
{
  return clamp(dot(N, L), 0.0, 1.0);
}

float blinn(vec3 N, vec3 H, float m)
{
  float NdotH = clamp(dot(N, H), 0.0, 1.0);
  return pow(NdotH, m * 2);;
}

float phong(vec3 N, vec3 L, vec3 V, float m)
{
  vec3 R = -reflect(L, N);
  float RdotV = clamp(dot(R, V), 0.0, 1.0);
  return pow(RdotV, m);
}


vec2 parallaxOffset(sampler2D heightMapAlpha, vec2 texCoords, vec3 viewDir)
{ 
    if(rpPomMaxHeight < 0.00001)
        return vec2(0,0);

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
    float currentDepthMapValue = texture(heightMapAlpha, currentTexCoords).a;
      
    int i = 0;
    while(currentLayerDepth < currentDepthMapValue && i < 40)
    {
        // shift texture coordinates along direction of P
        currentTexCoords += deltaTexCoords;
        // get depthmap value at current texture coordinates
        currentDepthMapValue = texture(heightMapAlpha, currentTexCoords).a;  
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
    float beforeDepth = texture(heightMapAlpha, prevTexCoords).a - currentLayerDepth + layerDepth;
 
    // interpolation of texture coordinates
    float weight = afterDepth / (afterDepth - beforeDepth);
    vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

    return finalTexCoords - texCoords;
}