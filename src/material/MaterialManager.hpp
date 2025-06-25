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
typedef uint32_t TargetGroupIndex;

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

private:
	struct MaterialData;
	struct TargetGroup;

	uint32_t m_materialCount = 0;

	vk::Device& m_device;
	vk::DescriptorPool m_pool;

	std::unique_ptr<ShaderEngine> m_shaderEngine;
	std::unordered_map<std::string_view, MirroredBuffer> m_globalBuffers;

	std::unordered_map<MaterialIndex, MaterialData> m_materialData;
	std::unordered_map<MaterialIndex, MaterialMetadata> m_materialMetadata;

	std::unordered_map<TargetGroupIndex, TargetGroup> m_targetGroups;

	ResourceManager& m_resourceManager;

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
	template <typename T>
	MaterialData createMaterialData(MaterialIndex index);
	template <typename T>
	TargetGroupIndex createTargetGroup();

	template <typename T>
	T getMaterialData(MaterialIndex index);

	void syncData(std::vector<MirroredBuffer> buffers);

public:
	MaterialManager(Instance& instance, ResourceManager& resourceManager);
	void update(uint8_t currentFrame);
	MaterialInstance instantiateMaterial(MaterialDescription& description);

	Material getMaterial(MaterialIndex index);

	template <typename T, typename F>
	void updateMaterialData(MaterialIndex index, F updateFunction);

	template <typename T, typename F>
	void updateGlobalBuffer(std::string_view name, F updateFunction);

	vk::DescriptorSet getStaticTextureSet() const;

	std::vector<MaterialIndex> getMaterials() const;
	const MaterialMetadata& getMaterialMetadata(MaterialIndex index) {
		assert(m_materialMetadata.contains(index));
		return m_materialMetadata[index];
	}
};

struct MaterialManager::MaterialData {
	PipelineIndex pipeline;
	TargetGroupIndex attachmentIndex;

	std::vector<MirroredBuffer> materialBuffers;
	vk::DescriptorSet materialSet;

	struct AttachmentBinding {
		enum class BindingType {
			RENDERTARGET_COLOR,
			RENDERTARGET_DEPTH,
			SAMPLER,
			IMAGE,
			BUFFER,
		};

		ImageHandle image;
		BufferHandle buffer;
	};

	std::vector<AttachmentBinding> bindings;
};

struct MaterialManager::MaterialMetadata {
	std::string_view name;
	PipelineIndex pipeline;
	std::vector<vk::DescriptorSetLayoutBinding> materialBindings;
	vk::DescriptorSetLayout materialLayout;
	std::vector<ResourceDependency> namedResourceDependencies;
	bool enabled = true;
};

struct MaterialManager::TargetGroup {
	std::unordered_map<std::string_view, ImageHandle> images;
	std::unordered_map<std::string_view, ImageHandle> buffers;
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