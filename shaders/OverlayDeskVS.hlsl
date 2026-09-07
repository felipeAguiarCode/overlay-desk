#include "Common.hlsli"

// Fullscreen triangle generated from SV_VertexID.
//
// PERFORMANCE.md section 4 and 5: no vertex buffer, no index buffer, no input layout, and
// therefore nothing to bind, allocate or recreate on resize. A single triangle also beats
// a quad because it avoids the diagonal seam where two triangles meet, which shows up as
// a visible line once distortion is applied.
//
//   id 0 -> uv (0,0) -> clip (-1, +1)
//   id 1 -> uv (2,0) -> clip (+3, +1)
//   id 2 -> uv (0,2) -> clip (-1, -3)

PixelInput main(uint vertexId : SV_VertexID)
{
    PixelInput output;

    const float2 uv = float2((vertexId << 1) & 2u, vertexId & 2u);
    output.uv = uv;
    output.position = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);

    return output;
}
