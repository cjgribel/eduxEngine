#version 410 core

layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec3 iCenter;
layout(location = 2) in float iSize;
layout(location = 3) in uint iColorABGR;
layout(location = 4) in vec4 iUvRect;

uniform mat4 uProjView;
uniform vec3 uCameraRight;
uniform vec3 uCameraUp;

out vec2 vLocalUV;
out vec2 vSampleUV;
flat out uint vColorABGR;

void main()
{
    vec3 worldPos = iCenter
        + (uCameraRight * aCorner.x + uCameraUp * aCorner.y) * iSize;
    gl_Position = uProjView * vec4(worldPos, 1.0);
    vLocalUV = aCorner + vec2(0.5);
    vSampleUV = iUvRect.xy + vLocalUV * iUvRect.zw;
    vColorABGR = iColorABGR;
}
