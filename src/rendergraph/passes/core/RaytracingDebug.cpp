#include "material/MaterialManager.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/ResourceIndex.hpp"
#include "rendergraph/ResourceUsage.hpp"
#include "rendergraph/SetupContext.hpp"
#include "rendergraph/passes/RenderPass.hpp"

struct RayTracingData {
	rendergraph::ResourceIndex accelerationStructure;
	rendergraph::ResourceIndex indexBuffer;
	rendergraph::ResourceIndex primitiveData;
	rendergraph::ResourceIndex vertexAttributes;
	rendergraph::ResourceIndex materialData;

	TaskIndex sceneData;
	TaskIndex outputTask;

	MaterialIndex material;
};

static Task::Dependencies setup(Task::SetupContext& context) {
	auto data = context.getData<RayTracingData>();
	auto cameraBuffer = context.getReference(data.sceneData, rendergraph::passes::core::SceneDataSlots::Camera);

	return {
		.inputs = {
                { cameraBuffer, ResourceUsage::Type::UniformBuffer },
    			{ data.accelerationStructure, ResourceUsage::Type::AccellerationStructureRead },
    			{ data.indexBuffer, ResourceUsage::Type::StorageBufferRead },
    			{ data.primitiveData, ResourceUsage::Type::StorageBufferRead },
    			{ data.vertexAttributes, ResourceUsage::Type::StorageBufferRead },
    			{ data.materialData, ResourceUsage::Type::StorageBufferRead },
	        },
		.outputs = { { context.getReference(data.outputTask, 0), ResourceUsage::Type::ColorAttachmentWrite } },
	};
}
static void build(Task::BuildContext& context) {
	if (context.scene.tlas == nullptr) return;

	RenderPass::Begin(context, AttachmentOp::ClearWrite, AttachmentOp::Read);
	RenderPass::QuadDraw(context, context.getData<RayTracingData>().material);
	RenderPass::End(context.commandBuffer);
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
	auto material = context.materialManager.registerGraphicMaterial(
		"raytracing_debug",
		{
			.vertex = { "resources/shaders/quad_vert.slang" },
			.fragment = { "resources/shaders/raytracing_debug.slang" },
		},
		{
			.colorAttachmentFormats = { vk::Format::eR8G8B8A8Unorm },
			.cullMode = vk::CullModeFlagBits::eNone,
		},
		{
			{ vk::DescriptorType::eUniformBuffer },
			{ vk::DescriptorType::eAccelerationStructureKHR },
			{ vk::DescriptorType::eStorageBuffer },
			{ vk::DescriptorType::eStorageBuffer },
			{ vk::DescriptorType::eStorageBuffer },
			{ vk::DescriptorType::eStorageBuffer },

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
