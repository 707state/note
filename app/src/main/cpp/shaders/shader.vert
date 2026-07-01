#version 450

// Vertex shader for the Vulkan renderer.
// - inPosition: vec3, location 0
// - inUV:       vec2, location 1
// The projection matrix lives in a UBO (binding 0); the per-draw model matrix
// and color tint are delivered through push constants so we can draw many
// instances / buttons without touching descriptor sets.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;

layout(location = 0) out vec2 fragUV;

layout(set = 0, binding = 0) uniform UBO {
    mat4 uProjection;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 uModel;
    vec4 uColor;
} pc;

void main() {
    fragUV = inUV;
    gl_Position = ubo.uProjection * pc.uModel * vec4(inPosition, 1.0);
}

