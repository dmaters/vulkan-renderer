module primitive;

public struct PrimitiveData {
	uint32_t vertexOffset;
	uint32_t indexOffset;
	uint32_t materialIndex;
};
public struct Vertex {
	float3 position;

	struct Attributes{
    	float3 normal;
    	float3 tangent;
    	float3 bitangent;
    	float2 texCoord;
	};
};

public struct VertexInput {
	float3 position;
	float3 normal;
	float3 tangent;
	float3 bitangent;
	float2 texCoord;
};
