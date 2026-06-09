#include <metal_stdlib>
using namespace metal;

// 顶点输入结构体：与 CPU 侧 VertexData 内存布局一致
struct VertexIn {
    float3 position;  // 位置 (x, y, z)
    float4 color;     // 颜色 (r, g, b, a)
};

// 顶点着色器输出 / 片元着色器输入
struct VertexOut {
    float4 position [[position]];
    float4 color;
};

vertex VertexOut vertex_main(
    uint vertexID           [[vertex_id]],
    device const VertexIn* vertices [[buffer(0)]]   // 对应 setVertexBuffer 槽位 0
) {
    VertexIn in = vertices[vertexID];

    VertexOut out;
    out.position = float4(in.position, 1.0);
    out.color    = in.color;
    return out;
}

fragment float4 fragment_main(VertexOut in [[stage_in]]) {
    return in.color;
}
