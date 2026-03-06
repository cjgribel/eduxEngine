#version 410 core

in vec2 vLocalUV;
in vec2 vSampleUV;
flat in uint vColorABGR;

uniform sampler2D uTexture;
uniform bool uUseTexture;
uniform bool uSoftCircle;
uniform bool uTextureKeyEnabled;
uniform vec3 uTextureKeyColor;
uniform float uTextureKeyThreshold;
uniform bool uTextureFlipV;

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
        vec2 centered = vLocalUV * 2.0 - vec2(1.0);
        float dist = length(centered);
        float alpha = 1.0 - smoothstep(0.80, 1.0, dist);
        color.a *= alpha;
        if (color.a <= 0.001)
            discard;
    }

    if (uUseTexture)
    {
        vec2 sample_uv = vSampleUV;
        if (uTextureFlipV)
            sample_uv.y = 1.0 - sample_uv.y;
        vec4 texel = texture(uTexture, sample_uv);
        if (uTextureKeyEnabled)
        {
            float keyDist = distance(texel.rgb, uTextureKeyColor);
            float keyAlpha = smoothstep(0.0, max(uTextureKeyThreshold, 0.0001), keyDist);
            texel.a *= keyAlpha;
        }
        color *= texel;
    }

    if (color.a <= 0.001)
        discard;

    FragColor = color;
}
