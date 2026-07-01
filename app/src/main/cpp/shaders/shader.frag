#version 450

// Fragment shader: samples the texture and multiplies by the per-draw color tint
// (push constant). The same pipeline is used for the textured robot (tint = white)
// and the solid-color buttons / grid lines (tint = button color over a 1x1 white texture).

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D uTexture;

layout(push_constant) uniform PushConstants {
    mat4 uModel;
    vec4 uColor;
} pc;

void main() {
    outColor = texture(uTexture, fragUV) * pc.uColor;
}

