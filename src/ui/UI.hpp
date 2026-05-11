#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <queue>
#include <unordered_map>
#include <vulkan/vulkan.hpp>

namespace UI {
struct UIData {
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
		float exposure;
	};
	CameraData cameraData;

	struct Performance {
		uint16_t frameRate;
		float cpuFrameTime;
		float gpuFrameTime;
		std::unordered_map<std::string_view, float> taskFrameTime;
	};
	Performance performanceData;

	struct System {
		std::string currentGpu;
	};

	System systemData;

	struct Utils {
		bool showControlsWindow = true;
	};

	Utils _utils;
};

void Setup();
void Render(vk::CommandBuffer commandBuffer);

extern UIData Data;
}  // namespace UI
