#include "BufferUpload.hpp"

void BufferUpload(TaskContext& context) {
	Buffer& origin = context.getInput<Buffer&>(0);
	Buffer& destination = context.getOutput<Buffer&>(0);

	vk::BufferCopy copy {
		.srcOffset = origin.allocation.offset +
		             origin.size / 3 * (context.currentFrame % 3),
		.size = origin.size / 3,
		
	};

	context.commandBuffer.copyBuffer(
		origin.buffer, destination.buffer, 1, &copy
	);
}
