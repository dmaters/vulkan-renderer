#include "../ComputePass.hpp"
#include "rendergraph/BuildContext.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/SetupContext.hpp"

struct SSRData {
	TaskIndex sceneData;
	TaskIndex gbuffer;
	TaskIndex hdrCopy;
	TaskIndex hiz;
	TaskIndex hdrOutput;

	MaterialIndex material;
};

static Task::Dependencies setup(Task::SetupContext& context) {
	auto data = context.getData<SSRData>();

	return {
	    .inputs = {
   		   {
                context.getReference(data.sceneData, rendergraph::passes::core::SceneDataSlots::Camera),
                ResourceUsage::Type::UniformBuffer,
            },
            {
                context.getReference(data.gbuffer, rendergraph::passes::core::GBufferSlots::Normal),
                ResourceUsage::Type::SampledRead,
            },
            {
				 context.getReference(data.gbuffer, rendergraph::passes::core::GBufferSlots::WorldPos),
				 ResourceUsage::Type::SampledRead,
			},
			{
                context.getReference(data.gbuffer, rendergraph::passes::core::GBufferSlots::RoughnessMetallic),
                ResourceUsage::Type::SampledRead,
            },
 			{
                context.getReference(data.hdrCopy, 0),
                ResourceUsage::Type::SampledRead,
            },
 			{
                context.getReference(data.hiz, 0),
                ResourceUsage::Type::SampledRead,
            },
	    },
		.outputs = { { context.getReference(data.hdrOutput, 0), ResourceUsage::Type::ShaderWrite } } };
}

static void build(Task::BuildContext& context) {
	context.scene.camera.getResolution();
	ComputePass(context, context.getData<SSRData>().material, glm::uvec3(context.scene.camera.getResolution() / 8, 1));
}

TaskIndex rendergraph::passes::core::ssr(
	PassBuildContext& context,
	TaskIndex sceneData,
	TaskIndex gbuffer,
	TaskIndex hdrCopy,
	TaskIndex hiz,
	TaskIndex hdrOutput
) {
	auto material = context.materialManager.registerComputeMaterial(
		"ssr",
		{ "resources/shaders/ssr.slang" },
		{
			{ vk::DescriptorType::eUniformBuffer },
			{ vk::DescriptorType::eSampledImage },
			{ vk::DescriptorType::eSampledImage },
			{ vk::DescriptorType::eSampledImage },
			{ vk::DescriptorType::eSampledImage },
			{ vk::DescriptorType::eSampledImage },
			{ vk::DescriptorType::eStorageImage },
		}
	);

	return context.renderGraph.addTask(
		"ssr",
		{
			.setup = setup,
			.build = build,
		},
		SSRData {
			.sceneData = sceneData,
			.gbuffer = gbuffer,
			.hdrCopy = hdrCopy,
			.hiz = hiz,
			.hdrOutput = hdrOutput,
			.material = material,
		}
	);
}
