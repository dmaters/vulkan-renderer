#include "rendergraph/BuildContext.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/SetupContext.hpp"

static Task::Dependencies setup(Task::SetupContext& context) {
	auto resolution = context.scene.camera.getResolution();

	uint32_t minSize = std::min(resolution.x, resolution.y);
	uint32_t mipLevels = std::floor(std::log2(minSize)) + 1;

	auto ssrchain = context.createImage(
		"SSRChain",
		{
			.width = static_cast<uint32_t>(resolution.x),
			.height = static_cast<uint32_t>(resolution.y),
			.depth = 1,
			.miplevels = mipLevels,
			.format = vk::Format::eR16G16B16A16Sfloat,
			.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc |
					 vk::ImageUsageFlagBits::eSampled,
		}
	);

	return { .inputs = { {
				 context.getReference(context.getData<TaskIndex>(), 0),
				 ResourceUsage::Type::TransferSrc,
			 } },
			 .outputs = { { ssrchain, ResourceUsage::Type::TransferDst } } };
}

static void build(Task::BuildContext& context) {
	auto hdrOutput = context.getInput<Image&>(0);
	auto ssrchain = context.getOutput<Image&>(0);
	std::size_t mips = std::log2(std::min(hdrOutput.size.width, hdrOutput.size.height));

	vk::ImageBlit blitRegion {
		.srcSubresource =
		    {
				.aspectMask = hdrOutput.getAspectFlags(),
				.mipLevel = 0,
				.layerCount = 1,
			},
		.dstSubresource = {
			.aspectMask = hdrOutput.getAspectFlags(),
			.mipLevel = 0,
			.layerCount = 1,
		},
	};

	blitRegion.srcOffsets[1] = vk::Offset3D {
		static_cast<int32_t>(hdrOutput.size.width),
		static_cast<int32_t>(hdrOutput.size.height),
		1,
	};

	blitRegion.dstOffsets[1] = vk::Offset3D {
		static_cast<int32_t>(ssrchain.size.width),
		static_cast<int32_t>(ssrchain.size.height),
		1,
	};
	context.commandBuffer.blitImage(
		hdrOutput.image,
		vk::ImageLayout::eTransferSrcOptimal,
		ssrchain.image,
		vk::ImageLayout::eTransferDstOptimal,
		1,
		&blitRegion,
		vk::Filter::eLinear
	);

	for (int mip = 1; mip < mips; mip++) {
		vk::ImageMemoryBarrier2 barrier {
			.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.dstAccessMask = vk::AccessFlagBits2::eTransferRead,
			.oldLayout = vk::ImageLayout::eTransferDstOptimal,
			.newLayout = vk::ImageLayout::eTransferSrcOptimal,
			.image = ssrchain.image,
			.subresourceRange =
		    {
			   .aspectMask = ssrchain.getAspectFlags(),
			   .baseMipLevel = static_cast<uint32_t>(mip - 1),
			   .layerCount = 1,
		   },
		};

		context.commandBuffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier });

		vk::ImageBlit blitRegion {
		.srcSubresource =
		    {
				.aspectMask = ssrchain.getAspectFlags(),
				.mipLevel = static_cast<uint32_t>(mip - 1),
				.layerCount = 1,
			},
		.dstSubresource = {
			.aspectMask = hdrOutput.getAspectFlags(),
			.mipLevel = static_cast<uint32_t>(mip),
			.layerCount = 1,
		},
	};

		blitRegion.srcOffsets[1] = vk::Offset3D {
			static_cast<int32_t>(hdrOutput.size.width >> (mip - 1)),
			static_cast<int32_t>(hdrOutput.size.height >> (mip - 1)),
			1,
		};

		blitRegion.dstOffsets[1] = vk::Offset3D {
			static_cast<int32_t>(ssrchain.size.width >> mip),
			static_cast<int32_t>(ssrchain.size.height >> mip),
			1,
		};
		context.commandBuffer.blitImage(
			ssrchain.image,
			vk::ImageLayout::eTransferSrcOptimal,
			ssrchain.image,
			vk::ImageLayout::eTransferDstOptimal,
			1,
			&blitRegion,
			vk::Filter::eLinear
		);
	}

	vk::ImageMemoryBarrier2 barrier {
		.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.srcAccessMask = vk::AccessFlagBits2::eTransferRead,
		.oldLayout = vk::ImageLayout::eTransferSrcOptimal,
		.newLayout = vk::ImageLayout::eTransferDstOptimal,
		.image = ssrchain.image,
		.subresourceRange =
	    {
			.aspectMask = ssrchain.getAspectFlags(),
			.baseMipLevel = 0,
			.levelCount =  static_cast<uint32_t>(mips - 1),
			.layerCount = 1,
	   },
	};

	context.commandBuffer.pipelineBarrier2({ .imageMemoryBarrierCount = 1, .pImageMemoryBarriers = &barrier });
}

TaskIndex rendergraph::passes::core::ssrChainGen(PassBuildContext& context, TaskIndex hdrOutput) {
	return context.renderGraph.addTask(
		"SSRChainGen",
		{
			.setup = setup,
			.build = build,
		},
		hdrOutput
	);
}
