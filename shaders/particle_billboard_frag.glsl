#version 410 core

in vec2 vUV;
flat in uint vColorABGR;

uniform sampler2D uTexture;
uniform bool uUseTexture;
uniform bool uSoftCircle;

out vec4 FragColor;

vec4 unpackABGR(uint colorABGR)
{
    float r = float((colorABGR >> 0u) & 0xFFu) / 255.0;
    float g = float((colorABGR >> 8u) & 0xFFu) / 255.0;
    float b = float((colorABGR >> 16u) & 0xFFu) / 255.0;
    float a = float((colorABGR >> 24u) & 0xFFu) / 255.0;
    return vec4(r, g, b, a);
}

void main()
{
    vec4 color = unpackABGR(vColorABGR);

    if (uSoftCircle)
    {
        vec2 centered = vUV * 2.0 - vec2(1.0);
        float dist = length(centered);
        float alpha = 1.0 - smoothstep(0.80, 1.0, dist);
        color.a *= alpha;
        if (color.a <= 0.001)
            discard;
    }

    if (uUseTexture)
    {
        color *= texture(uTexture, vUV);
    }

    if (color.a <= 0.001)
        discard;

    FragColor = color;
}

