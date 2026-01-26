#pragma once

#include <slang.h>

#include <cassert>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

#include "Pipeline.hpp"
#include "material/Pipeline.hpp"

typedef uint32_t PipelineIndex;

class ShaderEngine {
private:
	struct PipelineIndexFields {
		enum class Type : uint8_t {
			Graphic,
			Compute,
		};

		unsigned int type : 1;
		unsigned int index : 31;
	};

	slang::IGlobalSession* m_session;

	std::unordered_map<PipelineIndex, Pipeline> m_pipelines;

	std::unordered_map<PipelineIndex, std::vector<vk::DescriptorSetLayout>>
		m_layouts;

	std::unordered_map<PipelineIndex, GraphicPipelineConfiguration>
		m_renderPassConfigurations;
	std::unordered_map<PipelineIndex, GraphicPipelineModules> m_graphicModules;

	std::unordered_map<PipelineIndex, ShaderModule> m_computeModules;

	std::vector<std::pair<Pipeline, uint8_t>> m_retiredPipelines;
	std::unordered_map<std::filesystem::path, std::unordered_set<PipelineIndex>>
		m_modules;

	std::mutex m_mutex;
	std::condition_variable m_cv;
	bool m_monitorEnabled = false;
	std::thread m_monitorThread;

	std::unordered_map<std::filesystem::path, std::filesystem::file_time_type>
		m_lastEdited;

	void _monitor();

	std::optional<vk::ShaderModule> loadModule(
		const std::filesystem::path& path,
		std::string_view entryPoint,
		SlangStage stage
	);

	std::optional<Pipeline> buildGraphicPipeline(
		GraphicPipelineModules& modules,
		std::vector<vk::DescriptorSetLayout>& layouts,
		GraphicPipelineConfiguration& configuration
	);
	std::optional<Pipeline> buildComputePipeline(
		ShaderModule computeModule,
		std::vector<vk::DescriptorSetLayout>& layouts
	);
	void reloadPipeline(PipelineIndex index);

public:
	ShaderEngine();
	~ShaderEngine();

	PipelineIndex registerGraphicPipeline(
		GraphicPipelineModules modules,
		std::vector<vk::DescriptorSetLayout> layouts,
		GraphicPipelineConfiguration configuration
	);
	PipelineIndex registerComputePipeline(
		ShaderModule computeModule, std::vector<vk::DescriptorSetLayout> layouts
	);

	Pipeline getPipeline(PipelineIndex index) {
		assert(m_pipelines.contains(index));
		return m_pipelines[index];
	}

	void flushRetiredPipelines();
};
