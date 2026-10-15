// The desktop mirror: one full-window triangle strip whose pixel shader
// maps up to two draw rectangles of the window onto source regions of the
// stereo texture, and paints everything else opaque black.
//
// Two panels because "both eyes" with symmetric off shows each eye's own
// crop side by side; every other mode uses panel 0 only and leaves panel 1
// empty (left == right, so no pixel falls inside it).
//
// Compiled into mirror_shaders.inc (see its header for the command); the
// DLL never loads a shader compiler at run time.

cbuffer Mirror : register(b0)
{
    float4 Draw[2];     // window UV (0..1, v down): left, top, right, bottom
    float4 Source[2];   // stereo texture UV: left, top, right, bottom
};

// t1, where the original's mirror bound it: slot 0 is left as the game
// set it.
Texture2D    Stereo : register(t1);
SamplerState Linear : register(s0);   // bilinear, clamped

struct VsOut
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

// Vertex ids 0..3 as a strip covering the whole viewport.
VsOut vs_main(uint id : SV_VertexID)
{
    VsOut o;
    o.uv  = float2(id & 1, id >> 1);
    o.pos = float4(o.uv.x * 2.0 - 1.0, 1.0 - o.uv.y * 2.0, 0.0, 1.0);
    return o;
}

float4 ps_main(VsOut i) : SV_Target
{
    float3 rgb = float3(0.0, 0.0, 0.0);
    [unroll]
    for (int p = 1; p >= 0; --p)
    {
        const float4 d = Draw[p];
        if (i.uv.x >= d.x && i.uv.x < d.z && i.uv.y >= d.y && i.uv.y < d.w)
        {
            // Linear in each axis, top left to top left.
            const float2 t  = (i.uv - d.xy) / (d.zw - d.xy);
            const float2 uv = lerp(Source[p].xy, Source[p].zw, t);
            rgb = Stereo.SampleLevel(Linear, uv, 0).rgb;
        }
    }
    return float4(rgb, 1.0);   // opaque everywhere
}
