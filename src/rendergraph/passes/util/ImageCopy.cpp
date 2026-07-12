#include "rendergraph/BuildContext.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/SetupContext.hpp"

struct ImageCopyData {
	TaskIndex origin;
	std::size_t originSlot;
	TaskIndex destination;
	std::size_t destinationSlot;
};

static void build(Task::BuildContext& context) {
	auto origin = context.getInput<Image&>(0);
	auto dest = context.getOutput<Image&>(0);
	vk::ImageCopy2 region = {
        .srcSubresource = {
            .aspectMask = origin.getAspectFlags(),
            .layerCount = 1,
        },
        .dstSubresource = {
            .aspectMask = origin.getAspectFlags(),
            .layerCount = 1,
        },
        .extent = origin.size,
    };
	vk::CopyImageInfo2 info = {
		.srcImage = origin.image,
		.srcImageLayout = vk::ImageLayout::eTransferSrcOptimal,
		.dstImage = dest.image,
		.dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
		.regionCount = 1,
		.pRegions = &region,
	};
	context.commandBuffer.copyImage2(info);
}

TaskIndex rendergraph::passes::util::imageCopy(
	PassBuildContext& context,
	TaskIndex origin,
	std::size_t originSlot,
	TaskIndex destination,
	std::size_t destinationSlot
) {
	return context.renderGraph.addTask(
		"image_copy",
		{
			.setup = [](Task::SetupContext& context) -> Task::Dependencies {
				auto data = context.getData<ImageCopyData>();
				return {
					.inputs = { {
						context.getReference(data.origin, data.originSlot),
						ResourceUsage::Type::TransferSrc,
					} },
					.outputs = { {
						context.getReference(data.destination, data.destinationSlot),
						ResourceUsage::Type::TransferDst,
					} },
				};
			},
			.build = build,
		},
		ImageCopyData {
			.origin = origin,
			.originSlot = originSlot,
			.destination = destination,
			.destinationSlot = destinationSlot,
		}
	);
}
