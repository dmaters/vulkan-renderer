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
#include "utils/IndexMap.hpp"

typedef uint32_t PipelineIndex;

class ShaderEngine {
private:
	enum class PipelineType : uint8_t {
		Graphic,
		Compute,
		Raytracing,
	};

	slang::IGlobalSession* m_globalSession;
	slang::ISession* m_registerSession;

	IndexMap<PipelineIndex, Pipeline> m_pipelines;
	IndexMap<PipelineIndex, PipelineType> m_pipelineTypes;
	union PipelineModules {
		std::array<ShaderModule, 3> values;

		GraphicPipelineModules graphicsModules;
		ShaderModule computeModule;
		RaytracingPipelineModules raytracingModules;
	};

	IndexMap<PipelineIndex, PipelineModules> m_modules;

	IndexMap<PipelineIndex, std::array<vk::DescriptorSetLayout, 2>> m_layouts;
	IndexMap<PipelineIndex, GraphicPipelineConfiguration> m_renderPassConfigurations;

	std::vector<std::pair<Pipeline, uint8_t>> m_retiredPipelines;
	std::unordered_map<std::filesystem::path, std::unordered_set<PipelineIndex>> m_dependencies;

	std::mutex m_mutex;
	std::condition_variable m_cv;
	bool m_monitorEnabled = false;
	std::thread m_monitorThread;

	std::unordered_map<std::filesystem::path, std::filesystem::file_time_type> m_lastEdited;

	void _monitor();
	void reloadPipeline(PipelineIndex index);

public:
	ShaderEngine();
	~ShaderEngine();

	PipelineIndex registerGraphicPipeline(
		GraphicPipelineModules modules,
		std::array<vk::DescriptorSetLayout, 2> layouts,
		GraphicPipelineConfiguration configuration
	);
	PipelineIndex registerComputePipeline(ShaderModule computeModule, std::array<vk::DescriptorSetLayout, 2> layouts);
	PipelineIndex registerRaytracingPipeline(
		RaytracingPipelineModules raytracingModules, std::array<vk::DescriptorSetLayout, 2> layouts
	);

	Pipeline getPipeline(PipelineIndex index) { return m_pipelines[index]; }
	void flushRetiredPipelines();
};
