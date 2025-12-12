#include "MemoryBarrier.hpp"

#include "Task.hpp"
void MemoryBarrier(
	TaskContext& context,
	std::unordered_map<ResourceIndex, vk::BufferMemoryBarrier2>
		bufferResourceBarriers,
	std::unordered_map<ResourceIndex, vk::ImageMemoryBarrier2>
		imageResourceBarriers
) {
	std::vector<vk::BufferMemoryBarrier2> bufferBarriers;
	std::vector<vk::ImageMemoryBarrier2> imageBarriers;

	for (auto [index, barrier] : bufferResourceBarriers) {
		Buffer& buffer =
			context.resourceManager.getBuffer(context.buffers.at(index));
		barrier.buffer = buffer.buffer;
		barrier.size = buffer.size;
		bufferBarriers.push_back(barrier);
	}
	for (auto [index, barrier] : imageResourceBarriers) {
		Image& image =
			context.resourceManager.getImage(context.images.at(index));

		barrier.image = image.image;
		barrier.subresourceRange = {
			.aspectMask = Image::GetAspectFlags(image.format),
			.levelCount = 1,
			.layerCount = 1,
		};

		imageBarriers.push_back(barrier);
	}

	context.commandBuffer.pipelineBarrier2(
		vk::DependencyInfo {
			.bufferMemoryBarrierCount =
				static_cast<uint32_t>(bufferBarriers.size()),
			.pBufferMemoryBarriers = bufferBarriers.data(),
			.imageMemoryBarrierCount =
				static_cast<uint32_t>(imageBarriers.size()),
			.pImageMemoryBarriers = imageBarriers.data(),

		}
	);
}
