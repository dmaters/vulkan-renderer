#pragma once

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

#include "Instance.hpp"
#include "Material.hpp"
#include "ShaderEngine.hpp"
#include "resources/ResourceManager.hpp"

struct GlobalResources {
	struct Camera {
		glm::mat4 view;
		glm::mat4 projection;
	};

	Camera camera;
};
typedef uint32_t MaterialInstanceIndex;
typedef uint32_t MaterialIndex;

struct MaterialInstance {
	MaterialIndex index = 0;
	MaterialInstanceIndex instance = 0;
};

class MaterialManager {
private:
	uint32_t m_materialCount = 0;

	vk::Device& m_device;
	vk::DescriptorPool m_pool;

	std::unique_ptr<ShaderEngine> m_shaderEngine;
	std::unordered_map<MaterialIndex, Material> m_materials;
	std::unordered_map<PipelineIndex, MaterialIndex> m_pipelines;
	std::unordered_set<MaterialIndex> m_brokenMaterials;

	ResourceManager& m_resourceManager;

	vk::DescriptorSet m_emptySet;
	vk::DescriptorSet m_emptySetLayout;
	vk::DescriptorSetLayout m_globalSetLayout;
	std::array<vk::DescriptorSet, 3> m_globalSets;

	vk::Sampler m_linearSampler;

	BufferHandle m_cameraUBO;

	void createGlobalDescriptorSet();
	void createMaterial(MaterialDescription& description);

public:
	MaterialManager(Instance& instance, ResourceManager& resourceManager);
	void update(uint8_t currentFrame);

	MaterialInstance instantiateMaterial(MaterialDescription& description);

	Material& getMaterial(MaterialIndex index);
};
