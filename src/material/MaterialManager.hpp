#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "ShaderEngine.hpp"
#include "material/Pipeline.hpp"
#include "resources/ResourceManager.hpp"

using MaterialIndex = uint32_t;
class MaterialManager {
private:
	uint32_t m_currentTextureOffset = 0;

	vk::DescriptorPool m_pool;
	std::unique_ptr<ShaderEngine> m_shaderEngine;

	std::vector<PipelineIndex> m_pipelines;
	std::unordered_map<std::string_view, MaterialIndex> m_names;

	ResourceManager& m_resourceManager;
	ResourceManager::AllocationIndex m_materialDataAllocation;

	vk::DescriptorSetLayout m_textureSetLayout;
	vk::DescriptorSet m_textureSet;

	vk::DescriptorSetLayout m_emptySetLayout;
	vk::DescriptorSet m_emptySet;

	void createTextureDescriptorSet();

public:
	MaterialManager(ResourceManager& resourceManager);

	void update();

	MaterialIndex registerComputeMaterial(
		std::string_view name,
		ComputePipelineModule module,
		std::vector<vk::DescriptorSetLayoutBinding> bindings
	);
	MaterialIndex registerGraphicMaterial(
		std::string_view name,
		GraphicPipelineModules modules,
		std::vector<vk::DescriptorSetLayoutBinding> bindings,
		GraphicPipelineConfiguration renderpassConfig
	);
	Pipeline getPipeline(MaterialIndex index) {
		return m_shaderEngine->getPipeline(m_pipelines[index]);
	}

	MaterialIndex getMaterialIndex(std::string_view name) const {
		return m_names.at(name);
	}

	uint32_t registerTextureGroup(ResourceManager::AllocationIndex allocation);
	vk::DescriptorSet getTextureSet() const { return m_textureSet; }
	vk::DescriptorSet getEmptySet() const { return m_emptySet; }

	vk::DescriptorSetLayout getTextureSetLayout() const {
		return m_textureSetLayout;
	}
};
