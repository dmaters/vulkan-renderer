#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

#include "Instance.hpp"
#include "Material.hpp"
#include "ShaderEngine.hpp"
#include "resources/Buffer.hpp"
#include "resources/ResourceManager.hpp"

typedef uint32_t MaterialInstanceIndex;
typedef uint32_t MaterialIndex;

struct MirroredBuffer {
	BufferHandle localBuffer;
	BufferHandle deviceBuffer;
};

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

	vk::Device& m_device;
	vk::DescriptorPool m_pool;

	std::unique_ptr<ShaderEngine> m_shaderEngine;
	std::unordered_map<std::string_view, MirroredBuffer> m_globalBuffers;

	std::unordered_map<MaterialIndex, MaterialData> m_materialData;
	std::unordered_map<MaterialIndex, MaterialMetadata> m_materialMetadata;
	std::unordered_map<std::string_view, MaterialIndex> m_names;

	ResourceManager& m_resourceManager;

	vk::DescriptorSet m_emptySet;
	vk::DescriptorSetLayout m_emptySetLayout;
	vk::DescriptorSet m_globalSet;
	vk::DescriptorSetLayout m_globalSetLayout;

	std::vector<ImageHandle> m_staticTextureGroup;

	vk::Sampler m_linearSampler;

	BufferHandle m_cameraUBO;

	void createGlobalBuffers();
	void createGlobalDescriptorSet();

	template <typename T>
	MaterialIndex registerMaterial();
	template <typename T>
	MaterialData createMaterialData(MaterialIndex index);

	template <typename T>
	T getMaterialData(MaterialIndex index);

	void syncData(std::vector<MirroredBuffer> buffers);

public:
	MaterialManager(Instance& instance, ResourceManager& resourceManager);
	void update(uint8_t currentFrame);

	Material getMaterial(MaterialIndex index);
	MaterialIndex getMaterialIndex(std::string_view name) const {
		return m_names.at(name);
	}

	template <typename T, typename F>
	void updateMaterialData(MaterialIndex index, F updateFunction);

	template <typename T, typename F>
	void updateGlobalBuffer(std::string_view name, F updateFunction);

	uint32_t registerTextureGroup(const std::vector<ImageHandle>& textures);

	vk::DescriptorSet getGlobalSet() const { return m_globalSet; };

	std::vector<MaterialIndex> getMaterials() const;

	const std::unordered_map<MaterialIndex, MaterialMetadata>&
	getMaterialMetadatas() const {
		return m_materialMetadata;
	}
};

struct MaterialManager::MaterialData {
	PipelineIndex pipeline;
	std::vector<MirroredBuffer> materialBuffers;
	vk::DescriptorSet materialSet;
};

struct MaterialManager::MaterialMetadata {
	std::string_view name;
	PipelineIndex pipeline;

	std::vector<vk::DescriptorSetLayoutBinding> materialBindings;
	vk::DescriptorSetLayout materialLayout;

	std::vector<MaterialManager::ResourceDependency> namedResourceDependencies;

	bool enabled = true;
};

template <typename T, typename F>
void MaterialManager::updateMaterialData(
	MaterialIndex index, F updateFunction
) {
	T data = getMaterialData<T>(index);
	updateFunction(data);
	syncData(m_materialData[index].materialBuffers);
}

template <typename T, typename F>
void MaterialManager::updateGlobalBuffer(
	std::string_view name, F updateFunction
) {
	assert(m_globalBuffers.contains(name));
	MirroredBuffer mbuffer = m_globalBuffers[name];
	Buffer& buffer = m_resourceManager.getBuffer(mbuffer.localBuffer);

	assert(buffer.size == sizeof(T));

	updateFunction(*reinterpret_cast<T*>(buffer.allocation.address));
	syncData({ mbuffer });
}

struct MaterialManager::ResourceDependency {
	enum class Kind {
		RenderTarget,
		Sampler,
		Buffer,
	};

	std::string_view name;
	Kind kind;
	ResourceUsage usage;
};