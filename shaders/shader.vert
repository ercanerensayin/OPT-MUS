#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;    // projection * view * model  (Y-flip projeksiyonun icinde uygulanir)
    mat4 model;  // normalleri dunya uzayina tasimak icin
} push;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragNormalWorld;
layout(location = 2) out vec3 fragPositionWorld;
// Perspektif projeksiyonda gl_Position.w, kameraya olan gorus uzayi derinligidir.
// Kamera konumunu push constant ile tasimadan sis hesabi icin yeterlidir.
layout(location = 3) out float fragViewDepth;

void main() {
    vec4 worldPosition = push.model * vec4(inPosition, 1.0);

    gl_Position = push.mvp * vec4(inPosition, 1.0);
    fragViewDepth = gl_Position.w;

    // Modeller yalnizca donme + oteleme icerdigi (tekduze olmayan olcekleme yok)
    // icin mat3(model) yeterlidir. Tekduze olmayan olcekleme eklenirse burada
    // transpose(inverse(mat3(model))) kullanilmalidir.
    fragNormalWorld = normalize(mat3(push.model) * inNormal);
    fragPositionWorld = worldPosition.xyz;
    fragColor = inColor;
}
