#include <metal_stdlib>
using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float4 color;
};

vertex VertexOut vertex_main(uint vertexID [[vertex_id]]) {
    // 三角形顶点位置（NDC 坐标）
    const float2 positions[] = {
        float2( 0.0,  0.5),   // 顶部
        float2(-0.5, -0.5),   // 左下
        float2( 0.5, -0.5),   // 右下
    };

    // 顶点颜色（RGB）
    const float4 colors[] = {
        float4(1.0, 0.0, 0.0, 1.0),  // 红
        float4(0.0, 1.0, 0.0, 1.0),  // 绿
        float4(0.0, 0.0, 1.0, 1.0),  // 蓝
    };

    VertexOut out;
    out.position = float4(positions[vertexID], 0.0, 1.0);
    out.color = colors[vertexID];
    return out;
}

fragment float4 fragment_main(VertexOut in [[stage_in]]) {
    return in.color;
}
