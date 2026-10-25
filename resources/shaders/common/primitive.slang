struct PrimitiveData {
	uint32_t vertexOffset;
	uint32_t indexOffset;
	uint32_t materialIndex;
};


struct Vertex {
	float3 position;

	struct Attributes{
    	float3 normal;
    	float3 tangent;
    	float3 bitangent;
    	float2 texCoord;
	};
};
