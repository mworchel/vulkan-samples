//////////////
// TYPEDEFS //
//////////////
struct VertexInputType
{
    uint vertexId : SV_VertexId;
};

struct PixelInputType
{
    float4 position : SV_Position;
    float3 color : COLOR0;
};

static float2 positions[3] =
{
    float2(0.0f, -0.5f),
    float2(0.5f, 0.5f),
    float2(-0.5f, 0.5f)
};

static float3 colors[3] =
{
    float3(1.0, 0.0, 0.0),
    float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0)
};

////////////////////////////////////////////////////////////////////////////////
// Vertex Shader
////////////////////////////////////////////////////////////////////////////////
PixelInputType main(VertexInputType input)
{
    PixelInputType output;
    
    // Change the position vector to be 4 units for proper matrix calculations.
    float2 pos = positions[input.vertexId];
    output.position = float4(pos, 0.0f, 1.0f);
    output.color = colors[input.vertexId];

    return output;
}