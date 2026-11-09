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

	TaskIndex sceneData;
	TaskIndex outputTask;

	MaterialIndex material;

	rendergraph::ResourceIndex _SBT;
	uint64_t _accumulationFrame = 0;
};

static Task::Dependencies setup(Task::SetupContext& context) {
	auto& data = context.getData<RayTracingData>();
	auto cameraBuffer = context.getReference(data.sceneData, rendergraph::passes::core::SceneDataSlots::Camera);
	auto lightBuffer = context.getReference(data.sceneData, rendergraph::passes::core::SceneDataSlots::Lights);

	auto SBT = context.createBuffer(
		"raytracingSBT", RaytracingPass::GetSBTDescription(), ResourceManager::MemoryLocation::HostVisible
	);

	data._SBT = SBT;
	data._accumulationFrame = 0;
	return {
		.inputs = {
                { cameraBuffer, ResourceUsage::Type::UniformBuffer },
                { lightBuffer, ResourceUsage::Type::UniformBuffer },
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
	auto& data = context.getData<RayTracingData>();
	auto res = context.getOutput<Image&>(0).size;

	data._accumulationFrame++;
	RaytracingPass::TraceRays(
		context, data.material, data._SBT, glm::uvec3(res.width, res.height, 1), data._accumulationFrame
	);
}
TaskIndex rendergraph::passes::core::pathTracer(
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
		"path_tracer",
		{
			.raygen = { "resources/shaders/path_tracer.slang", "main_rgen" },
			.miss = { "resources/shaders/path_tracer.slang", "main_miss" },
			.closestHit = { "resources/shaders/path_tracer.slang", "main_chit" },
	},
		{
			{ vk::DescriptorType::eUniformBuffer },
			{ vk::DescriptorType::eUniformBuffer },
			{ vk::DescriptorType::eAccelerationStructureKHR },
			{ vk::DescriptorType::eStorageBuffer },
			{ vk::DescriptorType::eStorageBuffer },
			{ vk::DescriptorType::eStorageBuffer },
			{ vk::DescriptorType::eStorageBuffer },
			{ vk::DescriptorType::eStorageImage },

		}
	);
	return context.taskRegister.addTask(
		"path_tracer",
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
