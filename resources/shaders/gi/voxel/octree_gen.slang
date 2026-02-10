#include "octree.slang"

const static uint VOXEL_MAP_SIZE = 512;

struct PushConstants {
	float4x4 view;
	float4x4 proj;
};
[vk::push_constant] ConstantBuffer<PushConstants> _data;

struct VertexInput {
	[[vk::location(0)]]
	float3 position : POSITION;
	[[vk::location(1)]]
	float3 normal : NORMAL;
	[[vk::location(2)]]
	float3 tangent : TEXCOORD0;
	[[vk::location(3)]]
	float3 bitangent : TEXCOORD1;
	[[vk::location(4)]]
	float2 texCoord : TEXCOORD2;
};

struct VertexOutput {
	float4 position : SV_Position;
};

[shader("vertex")] VertexOutput main_vs(VertexInput input) {
	return { mul(_data.view, float4(input.position, 1)) };
};

///////////////////////

enum ProjectionPlane {
	XZ,
	XY,
	YZ
};

struct GeometryOutput {
	float4 fragPos : SV_Position;
	ProjectionPlane plane;
};

[shader("geometry")][maxvertexcount(9)] void main_gs(
	triangle VertexOutput vert[3], inout TriangleStream<GeometryOutput> stream
) {
	for (int i = 0; i < 3; i++) {
		float4 projected = mul(_data.proj, vert[i].position);
		stream.Append({ projected, ProjectionPlane.XZ });
	}
	stream.RestartStrip();

	for (int i = 0; i < 3; i++) {
		float4 projected = mul(_data.proj, vert[i].position.yzxw);
		stream.Append({ projected, ProjectionPlane.YZ });
	}
	stream.RestartStrip();

	for (int i = 0; i < 3; i++) {
		float4 projected = mul(_data.proj, vert[i].position.xzyw);
		stream.Append({ projected, ProjectionPlane.XY });
	}
	stream.RestartStrip();
};

///////////////////////

[vk_binding(0)] RWStructuredBuffer<Atomic<uint32_t>> _octree;

[shader("fragment")] void main_fs(GeometryOutput input) {
	Octree octree = Octree(_octree);

	float3 position = 0;
	switch (input.plane) {
		case ProjectionPlane::XZ:
			position = input.fragPos.xyz;
			break;

		case ProjectionPlane::YZ:
			position = input.fragPos.yzx;
			break;

		case ProjectionPlane::XY:
			position = input.fragPos.xzy;
			break;
	}
	position += float3(0.5, 0.5, 0);

	uint3 texelPos = uint3(position * float3(VOXEL_MAP_SIZE));

	octree.addLeaf(texelPos);
};
