
struct VertexOutput {
    float4 position :SV_Position;
    float2 tex_coord : TEXCOORD0;
};

[shader("vertex")]
VertexOutput main(uint vertexID : SV_VertexID){
    float2 vertices[3] =
		float2[3](
            float2(-1.0, -1.0),
            float2( 3.0, -1.0),
            float2(-1.0,  3.0)
    );

	float2 pos = vertices[vertexID];
    return {
        float4(pos, 0.0, 1),
        (pos * 0.5 + 0.5 ),
    };
}
