#pragma once

#include <cstdint>
#include <filesystem>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/glm.hpp>
#include <map>
#include <memory>
#include <optional>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

#include "Instance.hpp"
#include "Pipeline.hpp"
#include "resources/ResourceManager.hpp"

struct MaterialHandle : Handle<MaterialHandle> {};

class MaterialManager {
public:
	struct DescriptorSet {
		vk::DescriptorSet set;
		vk::DescriptorSetLayout layout;
	};
	struct MaterialDescription;
	struct GlobalResources;

private:
	struct MaterialInstance {
		Pipeline pipeline;
		DescriptorSet instanceSet;
		std::vector<ResourceManager::ImageHandle> textures;
	};

	vk::Device& m_device;
	vk::DescriptorPool m_pool;
	DescriptorSet m_globalDescriptor;
	std::unique_ptr<GlobalResources> m_globalResources;
	std::vector<ResourceManager::BufferHandle> m_globalResourceBuffers;
	std::map<MaterialHandle, MaterialInstance> m_materials;
	ResourceManager m_resourceManager;
	bool m_dirtyGlobalDescriptors;

	void setupGlobalResources();

public:
	MaterialManager(Instance& instance);
	MaterialHandle instantiateMaterial(MaterialDescription& description);
	const DescriptorSet& getGlobalDescriptorSet();

	GlobalResources& getGlobalResources() {
		m_dirtyGlobalDescriptors = true;
		return *m_globalResources;
	}

	inline const MaterialInstance& getMaterial(MaterialHandle handle) {
		return m_materials.at(handle);
	}
};

struct MaterialManager::MaterialDescription {
	struct Resource {
		uint32_t binding;
		uint32_t count;
		vk::DescriptorType type;
		vk::ShaderStageFlags stage;
		std::filesystem::path path;
	};
	std::filesystem::path vertex;
	std::filesystem::path fragment;
	std::vector<Resource> resources;

	struct DefaultMaterialTextures {
		std::filesystem::path albedo;
	};

	static MaterialDescription Default(DefaultMaterialTextures definition) {
		return { .vertex = "resources/shaders/main.vert.spv",
			     .fragment = "resources/shaders/main.frag.spv",
			     .resources = {
					 Resource { .binding = 0,
			                    .count = 1,
			                    .type = vk::DescriptorType::eSampledImage,
			                    .stage = vk::ShaderStageFlagBits::eFragment,
			                    .path = definition.albedo } } };
	}
};

struct MaterialManager::GlobalResources {
	struct CameraBuffer {
		glm::mat4 transformation;
		glm::mat4 projection;
	};
	CameraBuffer camera;
};

/// STAVO IMPLEMENTANDO LE IMAMAGINI, PENSA SOLO A QUELLO
// DI DEFAULT IMAGEVIEW SERVER PER TUTTO; CREA DEEFAULT POI PENSA NEGLI ALTRI
// CASI