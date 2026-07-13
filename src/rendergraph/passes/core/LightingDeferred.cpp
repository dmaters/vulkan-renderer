#include "../RenderPass.hpp"
#include "rendergraph/BuildContext.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/SetupContext.hpp"

struct LightingDeferredData {
	TaskIndex sceneData;
	TaskIndex gbuffer;
	TaskIndex shadows;
	TaskIndex skyLighting;
	TaskIndex hdrOutput;

	MaterialIndex material;
};
using namespace rendergraph::passes::core;

static Task::Dependencies setup(Task::SetupContext& context) {
	auto data = context.getData<LightingDeferredData>();

	return {
		.inputs = {
			{ context.getReference(data.sceneData, SceneDataSlots::Camera), ResourceUsage::Type::UniformBuffer },
			{ context.getReference(data.sceneData, SceneDataSlots::Lights), ResourceUsage::Type::UniformBuffer },
			{ context.getReference(data.gbuffer, GBufferSlots::Albedo), ResourceUsage::Type::SampledRead },
			{ context.getReference(data.gbuffer, GBufferSlots::Normal), ResourceUsage::Type::SampledRead },
			{ context.getReference(data.gbuffer, GBufferSlots::WorldPos), ResourceUsage::Type::SampledRead },
			{ context.getReference(data.gbuffer, GBufferSlots::RoughnessMetallic), ResourceUsage::Type::SampledRead },
			{ context.getReference(data.shadows, 0), ResourceUsage::Type::SampledRead },
			{ context.getReference(data.skyLighting, 0), ResourceUsage::Type::UniformBuffer },
		 },
		.outputs = {
			{ context.getReference(data.hdrOutput, 0), ResourceUsage::Type::ColorAttachmentWrite },
			{ context.getReference(data.gbuffer, GBufferSlots::Depth), ResourceUsage::Type::DepthStencilRead },
		 },
	};
}

TaskIndex rendergraph::passes::core::deferredLighting(
	PassBuildContext& context,
	TaskIndex hdrOutput,
	TaskIndex gbuffer,
	TaskIndex shadows,
	TaskIndex sceneData,
	TaskIndex skyLighting
) {
	return context.renderGraph.addTask(
		"lighting_deferred",
		{ .setup = setup,
		  .build =
			  [](Task::BuildContext& context) {
				  RenderPass::Begin(context, AttachmentOp::ClearWrite, AttachmentOp::Read);
				  RenderPass::QuadDraw(context, context.getData<LightingDeferredData>().material);
				  RenderPass::End(context.commandBuffer);
			  }

		},
		LightingDeferredData {
			.sceneData = sceneData,
			.gbuffer = gbuffer,
			.shadows = shadows,
			.skyLighting = skyLighting,
			.hdrOutput = hdrOutput,
			.material = context.materialManager.getMaterialIndex("lighting_deferred"),
		}

	);
}
