#include "../RenderPass.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/SetupContext.hpp"

struct FXAAData {
	TaskIndex sdrCopy;
	TaskIndex sdrOutput;
	MaterialIndex material;
};

TaskIndex rendergraph::passes::post_processing::fxaa(
	PassBuildContext& context, TaskIndex sdrCopy, TaskIndex sdrOutput
) {
	return context.renderGraph.addTask(
		"fxaa",
		{
			.setup = [](Task::SetupContext& context) -> Task::Dependencies {
				auto data = context.getData<FXAAData>();
				return {
					.inputs = {
					 { context.getReference(data.sdrCopy, 0), ResourceUsage::Type::ShaderRead },
					},
					.outputs = {
					 { context.getReference(data.sdrOutput, 0), ResourceUsage::Type::ColorAttachmentWrite },
					},
				};
			},
			.build =
				[](Task::BuildContext& context) {
					RenderPass::Begin(context, AttachmentOp::ClearWrite, AttachmentOp::Read);
					RenderPass::QuadDraw(context, context.getData<FXAAData>().material);
					RenderPass::End(context.commandBuffer);
				},
		},
		FXAAData {
			.sdrCopy = sdrCopy,
			.sdrOutput = sdrOutput,
			.material = context.materialManager.getMaterialIndex("fxaa"),
		}
	);
}
