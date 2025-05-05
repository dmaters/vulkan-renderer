
struct VertexOutput {
	float4 position : SV_Position;
	float4 fragPos : TEXCOORD0;
	float3 fragNormal : TEXCOORD1;
	float2 fragTexCoord : TEXCOORD2;
};