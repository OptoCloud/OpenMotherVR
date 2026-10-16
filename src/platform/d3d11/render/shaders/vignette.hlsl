// OpenMotherVR-Provenance: clean
// The comfort vignette and the snap-turn fade (spec 22 §4.3, §5): one
// triangle strip over the whole side-by-side stereo target whose pixel
// shader darkens each eye half about that eye's own forward axis. The
// output is a factor the blend state multiplies the target by (dest *
// src colour, alpha kept), so it never brightens; pixels with factor 1 are
// discarded and left untouched.
//
// Compiled into vignette_shaders.inc (see its header for the command); the
// DLL never loads a shader compiler at run time.

cbuffer Vignette : register(b0)
{
    float2 Tan;         // tangents of the eye's symmetric half FOV: h, v
    float2 Radius;      // inner, outer angle from the forward axis, radians
    float  Brightness;  // the snap-turn fade: 1 none, 0 black
    float3 Padding;
};

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
    // The position in this pixel's eye half: left half eye 0, right eye 1.
    const float2 e = float2(frac(i.uv.x * 2.0), i.uv.y);
    // Direction tangents of the pixel, then its angle from the axis.
    const float2 t = float2((2.0 * e.x - 1.0) * Tan.x, (1.0 - 2.0 * e.y) * Tan.y);
    const float a  = atan(length(t));
    const float f  = (1.0 - smoothstep(Radius.x, Radius.y, a)) * Brightness;
    if (f >= 1.0)
        discard;
    return float4(f, f, f, 1.0);
}
