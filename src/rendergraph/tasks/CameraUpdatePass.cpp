#include "CameraUpdatePass.hpp"

void CameraUpdatePass(TaskContext& context) {
	glm::mat4 view = context.scene.camera.getViewMatrix();
	glm::mat4 proj = context.scene.camera.getProjectionMatrix();

	MaterialDefinitions::Camera cameraView {
		.view = view,
		.projection = proj,
		.invView = glm::inverse(view),
		.invProj = glm::inverse(proj),
		.frustumPoints = context.scene.camera.getFrustumPoints(),
	};

	Buffer& projViewBuffer =
		context.resourceManager.getBuffer(context.buffers[context.inputs[0]]);

	context.commandBuffer.updateBuffer(
		projViewBuffer.buffer, 0, sizeof(cameraView), &cameraView
	);

	vk::BufferMemoryBarrier2 projViewUpdate {
		.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.buffer = projViewBuffer.buffer,
		.offset = 0,
		.size = projViewBuffer.size,

	};

	context.commandBuffer.pipelineBarrier2(
		vk::DependencyInfo {
			.bufferMemoryBarrierCount = 1,
			.pBufferMemoryBarriers = &projViewUpdate,
		}
	);
}
