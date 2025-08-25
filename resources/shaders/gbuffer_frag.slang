#include "base_transform.slang"
#include "bindings/geometry.slang"

struct FragmentOutput {
    float4 color;
    float4 world_space;
    float4 normal;
    float2 metallic_roughness;
};

[[vk::binding(3 ,0)]] Sampler2D _textures[512];



struct PrimitiveInstance {
    uint32_t albedo;
    uint32_t normal;
    uint32_t metallic_roughness;
};


[[vk::binding(0, 1)]] ConstantBuffer<PrimitiveInstance[512]> _instances;

float3 reconstructNormal(float2 xy) {
    float z = sqrt(max(0.0, 1.0 - xy.x*xy.x - xy.y*xy.y));
    return normalize(float3(xy, z));
}

[shader("fragment")]
FragmentOutput main(VertexOutput input){

    FragmentOutput output;

    PrimitiveInstance instance = _instances[uPrimitive.index];

    output.color = _textures[instance.albedo].Sample(input.texcoord);
    output.world_space = input.frag_position;
    

 
    float3 normal = reconstructNormal(_textures[instance.normal].Sample(input.texcoord).rg);
	float3x3 TBN = float3x3(input.tangent, input.bitangent,-input.normal);
	float3 localNormal = normalize(normal * 2.0 - 1.0);
	float3 transformedNormal = mul( localNormal,TBN);
    output.normal = float4(transformedNormal,1);
    output.metallic_roughness = _textures[instance.metallic_roughness].Sample(input.texcoord).gb;

    return output;
};
