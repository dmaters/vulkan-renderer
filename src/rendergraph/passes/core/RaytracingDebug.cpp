#include "material/MaterialManager.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/ResourceIndex.hpp"
#include "rendergraph/ResourceUsage.hpp"
#include "rendergraph/SetupContext.hpp"
#include "rendergraph/passes/RenderPass.hpp"

struct RayTracingData {
	rendergraph::ResourceIndex accelerationStructure;
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
	        },
		.outputs = { { context.getReference(data.outputTask, 0), ResourceUsage::Type::ColorAttachmentWrite } },
	};
}
static void build(Task::BuildContext& context) {
	if (context.scene.allocation == context.scene.asAllocation) return;

	RenderPass::Begin(context, AttachmentOp::ClearWrite, AttachmentOp::Read);
	RenderPass::QuadDraw(context, context.getData<RayTracingData>().material);
	RenderPass::End(context.commandBuffer);
}
TaskIndex rendergraph::passes::raytracing_debug(
	PassBuildContext& context,
	rendergraph::ResourceIndex accelerationStructure,
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
			.colorAttachmentFormats = { vk::Format::eR8G8B8A8Srgb },
			.cullMode = vk::CullModeFlagBits::eNone,
		},
		{
			{ vk::DescriptorType::eUniformBuffer },
			{ vk::DescriptorType::eAccelerationStructureKHR },
		}
	);
	return context.renderGraph.addTask(
		"raytracing_debug",
		{ .setup = setup, .build = build },
		RayTracingData {
			.accelerationStructure = accelerationStructure,
			.sceneData = sceneData,
			.outputTask = outputTask,
			.material = material,
		}
	);
}
