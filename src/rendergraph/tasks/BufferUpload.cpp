#include "BufferUpload.hpp"

#include "../BuildContext.hpp"
#include "../DataProvider.hpp"

void BufferUpload::build(Task::BuildContext& context, TaskIndex task) {
	BufferUpload& data = context.dataProvider.getData<BufferUpload>(task);

	Buffer& origin = context.getInput<Buffer&>(data.origin);
	Buffer& destination = context.getInput<Buffer&>(data.destination);

	vk::BufferCopy copy {
		.srcOffset = 0,
		.size = origin.size,
	};

	context.commandBuffer.copyBuffer(
		origin.buffer, destination.buffer, 1, &copy
	);
}
