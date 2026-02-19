
struct VertexOutput {
	float4 position : SV_Position;
	float4 frag_position : TEXCOORD0;
	float3 normal : TEXCOORD1;
	float3 tangent : TEXCOORD2;
	float3 bitangent : TEXCOORD3;
	float2 texcoord : TEXCOORD4;
	uint drawIndex;
};
