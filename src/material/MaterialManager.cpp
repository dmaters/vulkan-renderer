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

	registerMaterials();
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
	std::vector<vk::DescriptorType> descriptors
) {
	vk::DescriptorSetLayout layout = m_emptySetLayout;
	if (descriptors.size() > 0) {
		std::vector<vk::DescriptorSetLayoutBinding> bindings;

		for (int i = 0; i < descriptors.size(); i++)
			bindings.push_back(
				{
					.binding = static_cast<uint32_t>(i),
					.descriptorType = descriptors[i],
					.descriptorCount = 1,
					.stageFlags = vk::ShaderStageFlagBits::eAll,
				}
			);

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
	GraphicPipelineConfiguration renderpassConfig,
	std::vector<vk::DescriptorType> descriptors
) {
	vk::DescriptorSetLayout layout = m_emptySetLayout;
	if (descriptors.size() > 0) {
		std::vector<vk::DescriptorSetLayoutBinding> bindings;

		for (int i = 0; i < descriptors.size(); i++)
			bindings.push_back(
				{
					.binding = static_cast<uint32_t>(i),
					.descriptorType = descriptors[i],
					.descriptorCount = 1,
					.stageFlags = vk::ShaderStageFlagBits::eAll,
				}
			);

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

void MaterialManager::registerMaterials() {
	registerComputeMaterial(
		"transmittanceLUT",
		{ "resources/shaders/transmittanceLUT.slang" },
		{ vk::DescriptorType::eStorageImage }
	);

	registerComputeMaterial(
		"multiscatteringLUT",
		{ "resources/shaders/multiscatteringLUT.slang" },
		{
			vk::DescriptorType::eSampledImage,
			vk::DescriptorType::eStorageImage,
			vk::DescriptorType::eStorageBuffer,
		}
	);

	registerComputeMaterial(
		"skyviewLUT",
		{ "resources/shaders/skyviewLUT.slang" },
		{
			vk::DescriptorType::eUniformBuffer,
			vk::DescriptorType::eSampledImage,
			vk::DescriptorType::eSampledImage,
			vk::DescriptorType::eStorageImage,
		}
	);

	registerComputeMaterial(
		"sky_lighting",
		{ "resources/shaders/sky_lighting.slang" },
		{
			vk::DescriptorType::eSampledImage,
			vk::DescriptorType::eStorageBuffer,
		}
	);

	registerGraphicMaterial(
		"shadowmap",
		{
			.vertex = { "resources/shaders/shadow_vert.slang" },
			.fragment = { "resources/shaders/dummy_frag.slang" },
		},
		{
			.depthFormat = vk::Format::eD16Unorm,
			.depthWrite = true,
			.depthOp = vk::CompareOp::eLessOrEqual,
		},
		{
			vk::DescriptorType::eUniformBuffer,
			vk::DescriptorType::eUniformBuffer,
		}
	);

	registerGraphicMaterial(
		"gbuffer",
		{
			.vertex = {"resources/shaders/base_transform_vert.slang"},
			.fragment = {"resources/shaders/gbuffer_frag.slang"},
		},
		{
			.colorAttachmentFormats = {
				vk::Format::eR16G16B16A16Sfloat,
				vk::Format::eR16G16B16A16Sfloat,
				vk::Format::eR16G16B16A16Sfloat,
				vk::Format::eR16G16B16A16Sfloat,

			},
			.depthFormat = vk::Format::eD24UnormS8Uint,
			.depthWrite = true,
			.depthOp = vk::CompareOp::eLessOrEqual,
			.stencilEnabled = true,
			.stencilOp = {
				.failOp = vk::StencilOp::eKeep,
				.passOp = vk::StencilOp::eReplace,
				.compareOp = vk::CompareOp::eAlways,
				.compareMask = 0xFF,
				.writeMask = 0xFF,
				.reference = 1,
			}
		},
		{
		    vk::DescriptorType::eUniformBuffer,
			vk::DescriptorType::eStorageBuffer,
			vk::DescriptorType::eStorageBuffer,
			vk::DescriptorType::eStorageBuffer,
		}
	);

	registerGraphicMaterial(
		"lighting_deferred",
		{
			.vertex = { "resources/shaders/quad_vert.slang"         },
			.fragment = { "resources/shaders/lighting_deferred.slang" },
    },
		{
			.colorAttachmentFormats = { vk::Format::eR16G16B16A16Sfloat },
			.depthFormat = vk::Format::eD24UnormS8Uint,
			.cullMode = vk::CullModeFlagBits::eNone,
			.stencilEnabled = true,
			.stencilOp = { .failOp = vk::StencilOp::eKeep,
	                       .passOp = vk::StencilOp::eKeep,
	                       .compareOp = vk::CompareOp::eEqual,
	                       .compareMask = 0xFF,
	                       .writeMask = 0,
	                       .reference = 1 },
		},
		{
			vk::DescriptorType::eUniformBuffer,
			vk::DescriptorType::eUniformBuffer,
			vk::DescriptorType::eSampledImage,
			vk::DescriptorType::eSampledImage,
			vk::DescriptorType::eSampledImage,
			vk::DescriptorType::eSampledImage,
			vk::DescriptorType::eSampledImage,
			vk::DescriptorType::eUniformBuffer,
		}
	);

	registerGraphicMaterial(
	    "skybox",
    	{
    		.vertex = {"resources/shaders/quad_vert.slang"},
    		.fragment = {"resources/shaders/skybox.slang"},
    	},
    	{
    		.colorAttachmentFormats = {
    		    vk::Format::eR16G16B16A16Sfloat
    		},
    		.depthFormat = vk::Format::eD24UnormS8Uint,
    		.depthWrite = false,
    		.stencilEnabled = true,
    		.stencilOp =
    			{
    			.failOp = vk::StencilOp::eKeep,
    			.passOp = vk::StencilOp::eKeep,
    			.compareOp = vk::CompareOp::eEqual,
    			.compareMask = 0xFF,
    			.reference = 0,
    			}
    	},
        {
            vk::DescriptorType::eUniformBuffer,
            vk::DescriptorType::eSampledImage,
        }
	);

	registerComputeMaterial(
		"composition",
		{ "resources/shaders/composition.slang" },
		{
			vk::DescriptorType::eStorageImage,
			vk::DescriptorType::eStorageImage,
		}
	);
}
