#include "MaterialManager.hpp"

#include <cassert>
#include <cstdint>
#include <memory>
#include <ranges>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Instance.hpp"
#include "ShaderEngine.hpp"
#include "material/MaterialManager.hpp"
#include "resources/Buffer.hpp"
#include "resources/ResourceManager.hpp"

MaterialManager::MaterialManager(ResourceManager& resourceManager) :
	m_resourceManager(resourceManager) {
	vk::Device& device = Instance::Get().device;

	std::array<vk::DescriptorPoolSize, 2> sizes = {

		vk::DescriptorPoolSize {
								.type = vk::DescriptorType::eSampledImage,
								.descriptorCount = 256,
								},

		vk::DescriptorPoolSize {
								.type = vk::DescriptorType::eSampler,
								.descriptorCount = 4,
								}
	};

	vk::DescriptorPoolCreateInfo info {
		.flags = { vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind },
		.maxSets = 5,
		.poolSizeCount = sizes.size(),
		.pPoolSizes = sizes.data()
	};

	m_pool = device.createDescriptorPool(info);

	createTextureDescriptorSet();
	m_emptySetLayout = Instance::Get().device.createDescriptorSetLayout({});

	m_shaderEngine = std::make_unique<ShaderEngine>();
}

void MaterialManager::createTextureDescriptorSet() {
	vk::Device& device = Instance::Get().device;

	vk::Sampler linearClamp = device.createSampler(
		{
			.magFilter = vk::Filter::eLinear,
			.minFilter = vk::Filter::eLinear,
			.mipmapMode = vk::SamplerMipmapMode::eLinear,
			.addressModeU = vk::SamplerAddressMode::eClampToEdge,
			.addressModeV = vk::SamplerAddressMode::eClampToEdge,
			.addressModeW = vk::SamplerAddressMode::eClampToEdge,
			.maxLod = 100,
		}
	);
	vk::Sampler linearRepeat = device.createSampler(
		{
			.magFilter = vk::Filter::eLinear,
			.minFilter = vk::Filter::eLinear,
			.mipmapMode = vk::SamplerMipmapMode::eLinear,
			.addressModeU = vk::SamplerAddressMode::eRepeat,
			.addressModeV = vk::SamplerAddressMode::eRepeat,
			.addressModeW = vk::SamplerAddressMode::eRepeat,
			.maxLod = 100,
		}
	);

	vk::Sampler nearestClamp = device.createSampler(
		{
			.magFilter = vk::Filter::eNearest,
			.minFilter = vk::Filter::eNearest,
			.mipmapMode = vk::SamplerMipmapMode::eNearest,
			.addressModeU = vk::SamplerAddressMode::eClampToEdge,
			.addressModeV = vk::SamplerAddressMode::eClampToEdge,
			.addressModeW = vk::SamplerAddressMode::eClampToEdge,
			.maxLod = 100,
		}
	);

	vk::Sampler nearestRepeat = device.createSampler(
		{
			.magFilter = vk::Filter::eLinear,
			.minFilter = vk::Filter::eLinear,
			.mipmapMode = vk::SamplerMipmapMode::eLinear,
			.addressModeU = vk::SamplerAddressMode::eRepeat,
			.addressModeV = vk::SamplerAddressMode::eRepeat,
			.addressModeW = vk::SamplerAddressMode::eRepeat,
			.maxLod = 100,
		}
	);

	std::array<vk::DescriptorSetLayoutBinding, 2> bindings {
		vk::DescriptorSetLayoutBinding {
										.binding = 0,
										.descriptorType = vk::DescriptorType::eSampledImage,
										.descriptorCount = 256,
										.stageFlags = vk::ShaderStageFlagBits::eAll,
										},
		vk::DescriptorSetLayoutBinding {
										.binding = 1,
										.descriptorType = vk::DescriptorType::eSampler,
										.descriptorCount = 4,
										.stageFlags = vk::ShaderStageFlagBits::eAll,
										},
	};

	std::array<vk::DescriptorBindingFlags, 2> bindingFlags = {
		vk::DescriptorBindingFlagBits::ePartiallyBound |
			vk::DescriptorBindingFlagBits::eUpdateAfterBind,
		vk::DescriptorBindingFlags {},

	};

	vk::DescriptorSetLayoutBindingFlagsCreateInfo flagsInfo {
		.bindingCount = bindingFlags.size(),
		.pBindingFlags = bindingFlags.data()
	};

	vk::DescriptorSetLayoutCreateInfo layoutInfo {
		.pNext = &flagsInfo,
		.flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
		.bindingCount = bindings.size(),
		.pBindings = bindings.data(),
	};

	vk::DescriptorSetLayout layout =
		device.createDescriptorSetLayout(layoutInfo);

	vk::DescriptorSetAllocateInfo allocateInfo {
		.descriptorPool = m_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout,
	};

	auto set = device.allocateDescriptorSets(allocateInfo)[0];

	std::array<vk::DescriptorImageInfo, 4> samplerInfos {
		vk::DescriptorImageInfo { .sampler = linearClamp },
		vk::DescriptorImageInfo { .sampler = linearRepeat },
		vk::DescriptorImageInfo { .sampler = nearestClamp },
		vk::DescriptorImageInfo { .sampler = nearestRepeat },
	};

	vk::WriteDescriptorSet writeInfo {
		.dstSet = set,
		.dstBinding = 1,
		.dstArrayElement = 0,
		.descriptorCount = 4,
		.descriptorType = vk::DescriptorType::eSampler,
		.pImageInfo = samplerInfos.data(),
	};
	device.updateDescriptorSets({ writeInfo }, {});

	m_textureSet = set;
	m_textureSetLayout = layout;
}
MaterialIndex MaterialManager::registerComputeMaterial(
	std::string_view name,
	ShaderModule module,
	std::vector<vk::DescriptorSetLayoutBinding> bindings
) {
	vk::DescriptorSetLayout layout = m_emptySetLayout;
	if (bindings.size() > 0) {
		layout = Instance::Get().device.createDescriptorSetLayout(
			{
				.flags =
					vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
				.bindingCount = (uint32_t)bindings.size(),
				.pBindings = bindings.data(),
			}
		);
	}

	PipelineIndex pipeline = m_shaderEngine->registerComputePipeline(
		module, { layout, m_textureSetLayout }
	);

	MaterialIndex index = m_pipelines.size();
	m_pipelines.push_back(pipeline);
	m_names[name] = index;
	return index;
}
MaterialIndex MaterialManager::registerGraphicMaterial(
	std::string_view name,
	GraphicPipelineModules modules,
	std::vector<vk::DescriptorSetLayoutBinding> bindings,
	GraphicPipelineConfiguration renderpassConfig
) {
	vk::DescriptorSetLayout layout = m_emptySetLayout;
	if (bindings.size() > 0) {
		layout = Instance::Get().device.createDescriptorSetLayout(
			{
				.flags =
					vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
				.bindingCount = (uint32_t)bindings.size(),
				.pBindings = bindings.data(),
			}
		);
	}

	PipelineIndex pipeline = m_shaderEngine->registerGraphicPipeline(
		modules, { layout, m_textureSetLayout }, renderpassConfig
	);

	MaterialIndex index = m_pipelines.size();
	m_pipelines.push_back(pipeline);
	m_names[name] = index;
	return index;
}

void MaterialManager::update() { m_shaderEngine->flushRetiredPipelines(); }

uint32_t MaterialManager::registerTextureGroup(
	ResourceManager::DeviceAllocationIndex index
) {
	auto& textures = m_resourceManager.getImages(index);

	std::vector<vk::DescriptorImageInfo> info;
	for (auto& handle : textures) {
		Image& image = m_resourceManager.getImage(handle);

		info.push_back(
			{
				.imageView = image.view,
				.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			}
		);
	}

	vk::WriteDescriptorSet writeInfo = {
		.dstSet = m_textureSet,
		.dstBinding = 0,
		.dstArrayElement = 0,
		.descriptorCount = (uint32_t)textures.size(),
		.descriptorType = vk::DescriptorType::eSampledImage,
		.pImageInfo = info.data(),
	};

	Instance::Get().device.updateDescriptorSets({ writeInfo }, {});
	return 0;
}
