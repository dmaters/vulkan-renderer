#pragma once

#include <array>
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
#include "resources/Buffer.hpp"
#include "resources/ResourceManager.hpp"

struct GlobalResources {
	struct Camera {
		glm::mat4 view;
		glm::mat4 projection;
	};

	Camera camera;
};
struct DescriptorSet {
	vk::DescriptorSet set = nullptr;
	vk::DescriptorSetLayout layout = nullptr;
};
struct Material {
	Pipeline pipeline;
	DescriptorSet globalSet;
	DescriptorSet materialSet;
};

struct MaterialInstance {
	std::shared_ptr<Material> baseMaterial;
	DescriptorSet instanceSet;
};

class MaterialManager {
public:
	struct MaterialDescription;

private:
	vk::Device& m_device;
	vk::DescriptorPool m_pool;
	std::vector<std::shared_ptr<Material>> m_materials;

	ResourceManager& m_resourceManager;

	std::array<DescriptorSet, 3> m_globalSets;
	BufferHandle m_cameraUBO;

public:
	MaterialManager(Instance& instance, ResourceManager& resourceManager);
	void updateDescriptorSets(uint8_t currentFrame);

	MaterialInstance instantiateMaterial(MaterialDescription& description);
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
	std::vector<Resource> materialResources;
	std::vector<Resource> instanceResources;

	struct DefaultMaterialTextures {
		std::filesystem::path albedo;
	};

	static MaterialDescription Default(DefaultMaterialTextures definition) {
		return { .vertex = "resources/shaders/main.vert.spv",
			     .fragment = "resources/shaders/main.frag.spv",
			     .instanceResources = {
					{ .binding = 0,
			                    .count = 1,
			                    .type = vk::DescriptorType::eSampledImage,
			                    .stage = vk::ShaderStageFlagBits::eFragment,
							.path = definition.albedo, 
						}, 
					}, 
				};
	}
};
