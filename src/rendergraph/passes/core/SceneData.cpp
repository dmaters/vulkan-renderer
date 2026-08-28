#include <glm/glm.hpp>

#include "material/MaterialDefinitions.hpp"
#include "rendergraph/BuildContext.hpp"
#include "rendergraph/RenderGraphPasses.hpp"
#include "rendergraph/SetupContext.hpp"

static Task::Dependencies setup(Task::SetupContext& context) {
	auto cameraBuffer = context.createBuffer(
		"camera_buffer",
		ResourceManager::BufferDescription {
			.size = sizeof(Camera::ShaderObject),
			.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer,
		},
		ResourceManager::MemoryLocation::Device
	);
	auto lightBuffer = context.createBuffer(
		"light_buffer",
		ResourceManager::BufferDescription {
			.size = sizeof(Light::ShaderObject),
			.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer,
		},
		ResourceManager::MemoryLocation::Device
	);
	return  {
		.outputs = {
    		 { cameraBuffer, ResourceUsage::Type::TransferDst },
    		 { lightBuffer, ResourceUsage::Type::TransferDst },
	    },
	};
}

static void build(Task::BuildContext& context) {
	glm::mat4 view = context.scene.camera.getViewMatrix();
	glm::mat4 proj =
		context.scene.camera.getProjectionMatrix(context.renderingConfiguration.resolution, context.scene.size);

	Camera::ShaderObject cameraObject {
		.view = view,
		.projection = proj,
		.invView = glm::inverse(view),
		.invProj = glm::inverse(proj),
		.position = glm::vec4(context.scene.camera.position, 1),
		.direction = glm::vec4(context.scene.camera.getOrientation()[2], 0),
		.nearPlane = 0.1f,
		.farPlane = context.scene.camera.getFrustumSize(context.scene.size),
	};

	Buffer& cameraBuffer = context.getOutput<Buffer&>(rendergraph::passes::core::SceneDataSlots::Camera);

	context.commandBuffer.updateBuffer(cameraBuffer.buffer, 0, sizeof(Camera::ShaderObject), &cameraObject);

	Light::ShaderObject light =
		context.scene.lights[0].getShaderObject(context.scene.camera, cameraObject, context.scene.size);
	Buffer& lightBuffer = context.getOutput<Buffer&>(rendergraph::passes::core::SceneDataSlots::Lights);

	context.commandBuffer.updateBuffer(lightBuffer.buffer, 0, sizeof(Light::ShaderObject), &light);
}

TaskIndex rendergraph::passes::core::sceneData(PassBuildContext& context) {
	return context.renderGraph.addTask(
		"scene_data",
		{
			.setup = setup,
			.build = build,
		}
	);
}
