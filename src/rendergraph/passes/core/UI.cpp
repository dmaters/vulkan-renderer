#include "ui/UI.hpp"

#include "../RenderPass.hpp"
#include "rendergraph/BuildContext.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/SetupContext.hpp"

TaskIndex rendergraph::passes::core::ui(PassBuildContext& context, TaskIndex sdrOutput) {
	return context.renderGraph.addTask(
		"UI",
		{
			.setup = [](Task::SetupContext& context) -> Task::Dependencies {
				return {
					.outputs = {
						{
							context.getReference(context.getData<TaskIndex>(), 0),
							ResourceUsage::Type::ColorAttachmentWrite,
						},
					},
				};
			},
			.build =
				[](Task::BuildContext& context) {
					RenderPass::Begin(context, AttachmentOp::ReadWrite, AttachmentOp::ReadWrite);
					UI::Render(context.commandBuffer);
					RenderPass::End(context.commandBuffer);
				},
		},
		sdrOutput
	);
}
