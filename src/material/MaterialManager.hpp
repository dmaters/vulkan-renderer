#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include <variant>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Instance.hpp"
#include "Material.hpp"
#include "ShaderEngine.hpp"
#include "resources/Buffer.hpp"
#include "resources/ResourceManager.hpp"

typedef uint32_t MaterialInstanceIndex;
typedef uint32_t MaterialIndex;

struct MaterialInstance {
	MaterialIndex index = 0;
	MaterialInstanceIndex instance = 0;
};

class MaterialManager {
public:
	struct MaterialMetadata;
	struct ResourceDependency;

private:
	struct MaterialData;

	uint32_t m_materialCount = 0;

	vk::DescriptorPool m_pool;

	std::unique_ptr<ShaderEngine> m_shaderEngine;

	std::unordered_map<MaterialIndex, MaterialData> m_materialData;
	std::unordered_map<MaterialIndex, MaterialMetadata> m_materialMetadata;
	std::unordered_map<std::string_view, MaterialIndex> m_names;

	ResourceManager& m_resourceManager;
	ResourceManager::AllocationIndex m_materialDataAllocation;

	vk::DescriptorSet m_emptySet;
	vk::DescriptorSetLayout m_emptySetLayout;
	vk::DescriptorSet m_globalSet;
	vk::DescriptorSetLayout m_globalSetLayout;

	vk::Sampler m_linearSampler;

	BufferHandle m_cameraUBO;

	void createGlobalBuffers();
	void createGlobalDescriptorSet();

	template <typename T>
	MaterialIndex registerMaterial();

	void createMaterialData();

	template <typename T>
	T getMaterialData(MaterialIndex index);

	vk::DescriptorSet createSet(
		const std::vector<vk::DescriptorSetLayoutBinding>& bindings,
		const vk::DescriptorSetLayout layout,
		const std::vector<BufferHandle>& materialBuffersHandles
	);

public:
	MaterialManager(ResourceManager& resourceManager);

	void update(uint8_t currentFrame);

	Material getMaterial(MaterialIndex index);
	MaterialIndex getMaterialIndex(std::string_view name) const {
		return m_names.at(name);
	}

	uint32_t registerTextureGroup(ResourceManager::AllocationIndex allocation);

	template <typename T, typename F>
	void updateMaterialData(MaterialIndex index, F updateFunction);
	template <typename T, typename F>
	void updateGlobalBuffer(std::string_view name, F updateFunction);

	vk::Sampler getLinearSampler() const { return m_linearSampler; }
	vk::DescriptorSet getGlobalSet() const { return m_globalSet; };
	std::vector<MaterialIndex> getMaterials() const;

	const std::unordered_map<MaterialIndex, MaterialMetadata>&
	getMaterialMetadatas() const {
		return m_materialMetadata;
	}
};

struct MaterialManager::MaterialData {
	PipelineIndex pipeline;
	std::vector<BufferHandle> materialBuffers;
	vk::DescriptorSet materialSet;
};

struct MaterialManager::MaterialMetadata {
	std::string_view name;
	PipelineIndex pipeline;

	std::vector<vk::DescriptorSetLayoutBinding> materialBindings;
	vk::DescriptorSetLayout materialLayout;
	uint32_t materialBufferSize = 0;
};

template <typename T, typename F>
void MaterialManager::updateMaterialData(
	MaterialIndex index, F updateFunction
) {
	T data;

	updateFunction(data);
	uint32_t bufferSize = m_materialMetadata.at(index).materialBufferSize;

	m_resourceManager.queueBufferUpdate(
		m_materialData.at(index).materialBuffers.back(),
		[&data, bufferSize](std::byte* address) {
			memcpy(address, &data, bufferSize);
		}
	);
	// syncData(m_materialData[index].materialBuffers);
}

template <typename T>
T MaterialManager::getMaterialData(MaterialIndex index) {
	std::vector<BufferHandle>& buffers = m_materialData[index].materialBuffers;

	T data;
	/*
	for (int i = 0; i < buffers.size(); i++) {
	    Buffer& mirror = m_resourceManager.getBuffer(buffers[i].localBuffer);
	    void** field =
	    (void**)(reinterpret_cast<std::byte*>(&data) + i * sizeof(void*));
	    *field = mirror.allocation.address;
	};

	*/
	return {};
};
