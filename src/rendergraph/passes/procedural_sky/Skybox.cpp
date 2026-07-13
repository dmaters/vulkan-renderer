#include "../RenderPass.hpp"
#include "rendergraph/BuildContext.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/SetupContext.hpp"

struct SkyboxData {
	TaskIndex sceneData;
	TaskIndex skyviewLUT;
	TaskIndex hdrOutput;
	TaskIndex gbuffer;

	MaterialIndex material;
};
TaskIndex rendergraph::passes::procedural_sky::skybox(
	PassBuildContext& context, TaskIndex sceneData, TaskIndex skyviewLUT, TaskIndex hdrOutput, TaskIndex gbuffer
) {
	return context.renderGraph.addTask(
		"skybox",
		{

			.setup = [](Task::SetupContext& context) -> Task::Dependencies {
				auto data = context.getData<SkyboxData>();

				return {
					.inputs = {
                        {
							context.getReference(data.sceneData, core::SceneDataSlots::Camera),
							ResourceUsage::Type::UniformBuffer
						}, {
							context.getReference(data.skyviewLUT, 0),
							ResourceUsage::Type::SampledRead,
						},
					},
					.outputs = {
                        {
                            context.getReference(data.hdrOutput, 0),
    			              ResourceUsage::Type::ColorAttachmentWrite,
                        },
                        {
                            context.getReference(data.gbuffer, core::GBufferSlots::Depth),
    			              ResourceUsage::Type::DepthStencilRead
                        },
					 }
				};
			},

			.build =
				[](Task::BuildContext& context) {
					RenderPass::Begin(context, AttachmentOp::ReadWrite, AttachmentOp::Read);
					RenderPass::QuadDraw(context, context.getData<SkyboxData>().material);
					RenderPass::End(context.commandBuffer);
				},
		},
		SkyboxData {
			.sceneData = sceneData,
			.skyviewLUT = skyviewLUT,
			.hdrOutput = hdrOutput,
			.gbuffer = gbuffer,
			.material = context.materialManager.getMaterialIndex("skybox"),
		}
	);
}
