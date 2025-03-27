#pragma once

#include <array>
#include <filesystem>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

struct Pipeline {
	vk::Pipeline pipeline;
	vk::PipelineLayout pipelineLayout;
};

class PipelineBuilder {
private:
	vk::Pipeline m_pipeline;
	vk::PipelineLayout m_pipelineLayout;

public:
	struct PipelineBuildInfo {
		vk::Device& device;

		std::filesystem::path vertex;
		std::filesystem::path fragment;
		std::vector<vk::DescriptorSetLayout> layouts;
	};

	static Pipeline DefaultPipeline(const PipelineBuildInfo& info);
};
