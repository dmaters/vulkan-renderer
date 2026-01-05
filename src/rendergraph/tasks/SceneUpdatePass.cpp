#include "SceneUpdatePass.hpp"

#include <glm/glm.hpp>

#include "TaskContext.hpp"
#include "material/MaterialDefinitions.hpp"
void SceneUpdatePass(TaskContext& context) {
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

	MaterialDefinitions::Light light =
		context.scene.lights[0].getShaderObject(context.scene.camera);
	Buffer& lightBuffer = context.getOutput<Buffer&>(1);

	context.commandBuffer.updateBuffer(
		lightBuffer.buffer, 0, sizeof(MaterialDefinitions::Light), &light
	);
}
