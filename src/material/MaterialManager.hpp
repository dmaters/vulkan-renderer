#pragma once

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

#include "Instance.hpp"
#include "Material.hpp"
#include "resources/ResourceManager.hpp"

struct GlobalResources {
	struct Camera {
		glm::mat4 view;
		glm::mat4 projection;
	};

	Camera camera;
};

class MaterialManager {
public:
private:
	vk::Device& m_device;
	vk::DescriptorPool m_pool;
	std::vector<std::shared_ptr<Material>> m_materials;
	ResourceManager& m_resourceManager;

	vk::Sampler m_linearSampler;

	// std::unordered_map<
	// 	std::pair<std::filesystem::path, std::filesystem::path>,
	// 	uint32_t>
	// 	m_materialCache;

	std::array<DescriptorSet, 3> m_globalSets;
	BufferHandle m_cameraUBO;

	uint32_t createMaterial(MaterialDescription& description);

public:
	MaterialManager(Instance& instance, ResourceManager& resourceManager);
	void updateDescriptorSets(uint8_t currentFrame);

	MaterialInstance instantiateMaterial(MaterialDescription& description);
	inline std::shared_ptr<Material> getBaseMaterial() {
		return m_materials[0];
	}
};
