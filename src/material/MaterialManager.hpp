#pragma once

#include <cstddef>
#include <cstdint>
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

	uint32_t m_materialCount = 1;

	vk::DescriptorPool m_pool;

	std::unique_ptr<ShaderEngine> m_shaderEngine;

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

	MaterialData createMaterialData(MaterialIndex index);

	template <typename T>
	T getMaterialData(MaterialIndex index);

	void syncData(std::vector<MirroredBuffer> buffers);

	vk::DescriptorSet createSet(
		std::vector<vk::DescriptorSetLayoutBinding>& bindings,
		vk::DescriptorSetLayout layout,
		std::vector<BufferHandle>& materialBuffersHandles
	);

public:
	MaterialManager(ResourceManager& resourceManager);

	void update(uint8_t currentFrame);

	Material getMaterial(MaterialIndex index);
	MaterialIndex getMaterialIndex(std::string_view name) const {
		return m_names.at(name);
	}

	uint32_t registerTextureGroup(const std::vector<ImageHandle>& textures);

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
	std::vector<MirroredBuffer> materialBuffers;
	vk::DescriptorSet materialSet;
};

struct MaterialManager::ResourceDependency {
	enum class Kind {
		RenderTarget,
		Sampler,
		Buffer,
	};

	std::string_view name;
	Kind kind;
	ResourceUsage usage;
	std::optional<vk::ImageLayout> requiredLayout;
};

struct MaterialManager::MaterialMetadata {
	typedef std::variant<std::string_view, uint32_t> MaterialBuffer;

	std::string_view name;
	PipelineIndex pipeline;

	std::vector<vk::DescriptorSetLayoutBinding> materialBindings;
	vk::DescriptorSetLayout materialLayout;
	std::vector<MaterialBuffer> materialBuffers;
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

template <typename T>
T MaterialManager::getMaterialData(MaterialIndex index) {
	if (!m_materialData.contains(index)) {
		m_materialData[index] = createMaterialData(index);
	}
	std::vector<MirroredBuffer>& buffers =
		m_materialData[index].materialBuffers;

	T data;
	for (int i = 0; i < buffers.size(); i++) {
		Buffer& mirror = m_resourceManager.getBuffer(buffers[i].localBuffer);
		void** field =
			(void**)(reinterpret_cast<std::byte*>(&data) + i * sizeof(void*));
		*field = mirror.allocation.address;
	};
	return data;
};
