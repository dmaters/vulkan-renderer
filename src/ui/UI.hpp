#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace UI {
struct UIParameters {
	struct Scene {
		std::string scenePath;
		uint16_t primitiveCount;
		uint16_t gbufferCount;
	};

	Scene sceneData;

	struct Lighting {
		float sunAngleRad = glm::radians(20.0f);
		float sunStrength;
	};

	Lighting lightingData;

	struct CameraData {
		bool orbitalCamera;
		float cameraSpeed;
	};

	CameraData cameraData;
};
void Render(vk::CommandBuffer commandBuffer, UIParameters& parameters);
}  // namespace UI
