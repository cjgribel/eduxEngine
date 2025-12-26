#version 410 core

uniform sampler2D diffuseTexture;
uniform sampler2D normalTexture;
uniform sampler2D specularTexture;
uniform sampler2D opacityTexture;
uniform samplerCube cubeTexture;

uniform int has_diffuseTexture;
uniform int has_normalTexture;
uniform int has_specularTexture;
uniform int has_opacityTexture;
uniform int has_cubemap;

uniform vec3 lightpos;
uniform vec3 lightColor;
uniform vec3 eyepos;
uniform vec3 Ka;
uniform vec3 Kd;
uniform vec3 Ks;
uniform float shininess;
// uniform vec3 ucolor; // !!!

in vec3 wpos;
in vec2 texcoord;
in vec3 normal;
in vec3 tangent;
in vec3 binormal;
in vec3 color;
out vec4 fragcolor;

void main()
{
    // fragcolor = vec4(Kd, 1); return;

   vec3 N = normal;
   vec2 texflip = vec2(texcoord.x, texcoord.y);
   vec3 V = normalize(eyepos - wpos);
   vec3 L = normalize(lightpos - wpos);
   vec3 C = Kd;
   vec3 S = Ks;

   if (has_opacityTexture > 0)
   {
       if (texture(opacityTexture, texflip).x < 0.5)
           discard;
   }

   if (has_diffuseTexture > 0)
   {
       C = texture(diffuseTexture, texflip).rgb;
   }

   if (has_specularTexture > 0)
   {
       S = texture(specularTexture, texflip).rgb;
   }

   if (has_normalTexture > 0)
   {
       mat3 TBN = mat3(tangent, binormal, normal);
       vec3 bnormal = texture(normalTexture, texflip).xyz * 2.0 - 1.0;
       N = normalize( TBN * bnormal );
    //    fragcolor = vec4(N*0.5+0.5, 1.0); return;
   }

   vec3 R = reflect(-L, N);
   float ldot = max(0.0, dot(N, L));
   float rdot = max(0.0, dot(R, V));

//    if (has_reflectivetex > 0 && has_cubemap > 0)
//    {
//        float refl = texture(reflectiveTexture, texflip).x;
//        vec3 cubec = texture(cubeTexture, reflect(-V, N)).xyz;
//        C = cubec*refl + C*(1.0-refl);
//    }

   vec3 CC = (C*0.5 + C*ldot + S*pow(rdot, 20)) * lightColor * 1;
   
//    CC = CC / (CC + vec3(1.0));
   
   // Gamma correcton
    CC = pow(CC, vec3(1.0/1.4));

   fragcolor = vec4(CC, 1);
}
