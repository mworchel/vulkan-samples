//////////////
// TYPEDEFS //
//////////////
struct VertexInputType
{
    uint vertexId : SV_VertexId;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
};

static float2 positions[3] = {
    float2(0.0f, -0.5f),
    float2(0.5f,  0.5f),
    float2(-0.5f, 0.5f)
};

////////////////////////////////////////////////////////////////////////////////
// Vertex Shader
////////////////////////////////////////////////////////////////////////////////
PixelInputType main(VertexInputType input)
{
    PixelInputType output;
    
    // Change the position vector to be 4 units for proper matrix calculations.
    float2 pos = positions[input.vertexId];
    output.position = float4(pos.x, pos.y, 0.0f, 1.0f);
    
    return output;
}