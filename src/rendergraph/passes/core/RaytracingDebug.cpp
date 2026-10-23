#include "Instance.hpp"
#include "material/MaterialManager.hpp"
#include "rendergraph/BuildContext.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/ResourceIndex.hpp"
#include "rendergraph/ResourceUsage.hpp"
#include "rendergraph/SetupContext.hpp"
#include "rendergraph/passes/RaytracingPass.hpp"

struct RayTracingData {
	rendergraph::ResourceIndex accelerationStructure;
	rendergraph::ResourceIndex indexBuffer;
	rendergraph::ResourceIndex primitiveData;
	rendergraph::ResourceIndex vertexAttributes;
	rendergraph::ResourceIndex materialData;

	rendergraph::ResourceIndex SBT;
	TaskIndex sceneData;
	TaskIndex outputTask;

	MaterialIndex material;
};

static Task::Dependencies setup(Task::SetupContext& context) {
	auto& data = context.getData<RayTracingData>();
	auto cameraBuffer = context.getReference(data.sceneData, rendergraph::passes::core::SceneDataSlots::Camera);

	auto SBT = context.createBuffer(
		"raytracingSBT", RaytracingPass::GetSBTDescription(), ResourceManager::MemoryLocation::HostVisible
	);

	data.SBT = SBT;
	return {
		.inputs = {
                { cameraBuffer, ResourceUsage::Type::UniformBuffer },
    			{ data.accelerationStructure, ResourceUsage::Type::AccellerationStructureRead },
    			{ data.indexBuffer, ResourceUsage::Type::StorageBufferRead },
    			{ data.primitiveData, ResourceUsage::Type::StorageBufferRead },
    			{ data.vertexAttributes, ResourceUsage::Type::StorageBufferRead },
    			{ data.materialData, ResourceUsage::Type::StorageBufferRead },
    			{ SBT, ResourceUsage::Type::ShaderBindingTable },
	        },
		.outputs = { { context.getReference(data.outputTask, 0), ResourceUsage::Type::ShaderWrite } },
	};
}
static void build(Task::BuildContext& context) {
	auto data = context.getData<RayTracingData>();
	auto res = context.getOutput<Image&>(0).size;
	RaytracingPass::TraceRays(context, data.material, data.SBT, glm::uvec3(res.width, res.height, 1));
}
TaskIndex rendergraph::passes::raytracing_debug(
	PassBuildContext& context,
	rendergraph::ResourceIndex accelerationStructure,
	rendergraph::ResourceIndex indexBuffer,
	rendergraph::ResourceIndex primitiveData,
	rendergraph::ResourceIndex vertexAttributes,
	rendergraph::ResourceIndex materialData,
	TaskIndex sceneData,
	TaskIndex outputTask
) {
	auto material = context.materialManager.registerRaytracingMaterial(
		"raytracing_debug",
		{
			.raygen = { "resources/shaders/raytracing_debug.slang", "main_rgen" },
			.miss = { "resources/shaders/raytracing_debug.slang", "main_miss" },
			.closestHit = { "resources/shaders/raytracing_debug.slang", "main_chit" },
	},
		{
			{ vk::DescriptorType::eUniformBuffer },
			{ vk::DescriptorType::eAccelerationStructureKHR },
			{ vk::DescriptorType::eStorageBuffer },
			{ vk::DescriptorType::eStorageBuffer },
			{ vk::DescriptorType::eStorageBuffer },
			{ vk::DescriptorType::eStorageBuffer },
			{ vk::DescriptorType::eStorageImage },

		}
	);
	return context.renderGraph.addTask(
		"raytracing_debug",
		{ .setup = setup, .build = build },
		RayTracingData {
			.accelerationStructure = accelerationStructure,
			.indexBuffer = indexBuffer,
			.primitiveData = primitiveData,
			.vertexAttributes = vertexAttributes,
			.materialData = materialData,
			.sceneData = sceneData,
			.outputTask = outputTask,
			.material = material,
		}
	);
}
