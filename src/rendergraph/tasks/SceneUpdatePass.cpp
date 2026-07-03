#include "SceneUpdatePass.hpp"

#include <glm/glm.hpp>

#include "../BuildContext.hpp"
#include "../SetupContext.hpp"
#include "material/MaterialDefinitions.hpp"

void SceneData::Setup(Task::SetupContext& context) {
	auto cameraBuffer = context.createBuffer(
		"camera_buffer",
		ResourceManager::BufferDescription {
			.size = sizeof(MaterialDefinitions::Camera),
			.usage = vk::BufferUsageFlagBits::eTransferDst |
	                 vk::BufferUsageFlagBits::eUniformBuffer,
		},
		ResourceManager::MemoryLocation::Device
	);
	auto lightBuffer = context.createBuffer(
		"light_buffer",
		ResourceManager::BufferDescription {
			.size = sizeof(MaterialDefinitions::Light),
			.usage = vk::BufferUsageFlagBits::eTransferDst |
	                 vk::BufferUsageFlagBits::eUniformBuffer,
		},
		ResourceManager::MemoryLocation::Device
	);

	context.registerOutput(cameraBuffer, ResourceUsage::Type::TransferDst);
	context.registerOutput(lightBuffer, ResourceUsage::Type::TransferDst);
}
void SceneData::Build(Task::BuildContext& context) {
	glm::mat4 view = context.scene.camera.getViewMatrix();
	glm::mat4 proj = context.scene.camera.getProjectionMatrix();

	MaterialDefinitions::Camera cameraView {
		.view = view,
		.projection = proj,
		.invView = glm::inverse(view),
		.invProj = glm::inverse(proj),
	};

	Buffer& cameraBuffer = context.getOutput<Buffer&>(0);

	context.commandBuffer.updateBuffer(
		cameraBuffer.buffer, 0, sizeof(cameraView), &cameraView
	);

	MaterialDefinitions::Light light = context.scene.lights[0].getShaderObject(
		context.scene.camera, context.scene.size
	);
	Buffer& lightBuffer = context.getOutput<Buffer&>(1);

	context.commandBuffer.updateBuffer(
		lightBuffer.buffer, 0, sizeof(MaterialDefinitions::Light), &light
	);
}
