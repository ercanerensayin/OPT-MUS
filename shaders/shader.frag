#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragNormalWorld;
layout(location = 2) in vec3 fragPositionWorld;
layout(location = 3) in float fragViewDepth;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    mat4 model;
} push;

layout(location = 0) out vec4 outColor;

// Gecici sabit isik: ileride descriptor set ile sahne uniform'una tasinacak.
const vec3 kLightDirection = normalize(vec3(-0.45, -1.0, -0.35));
const vec3 kLightColor = vec3(1.0, 0.97, 0.90);
const vec3 kSkyColor = vec3(0.35, 0.42, 0.55);
const vec3 kGroundBounce = vec3(0.16, 0.14, 0.12);

void main() {
    vec3 normal = normalize(fragNormalWorld);

    float diffuse = max(dot(normal, -kLightDirection), 0.0);

    // Yarim-lambert benzeri yumusak ortam: gokyuzu ustten, zemin yansimasi alttan.
    float hemisphere = normal.y * 0.5 + 0.5;
    vec3 ambient = mix(kGroundBounce, kSkyColor, hemisphere) * 0.45;

    vec3 lit = fragColor * (ambient + kLightColor * diffuse);

    // Uzaklastikca gokyuzu rengine karisan basit sis: derinlik algisini guclendirir.
    float fog = clamp(1.0 - exp(-fragViewDepth * 0.012), 0.0, 1.0);
    lit = mix(lit, kSkyColor * 0.25, fog * 0.6);

    outColor = vec4(lit, 1.0);
}
