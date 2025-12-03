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
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>

#include "Pipeline.hpp"
#include "material/Pipeline.hpp"

typedef uint32_t PipelineIndex;

class ShaderEngine {
private:
	slang::IGlobalSession* m_session;

	std::unordered_map<PipelineIndex, Pipeline> m_pipelines;
	std::unordered_map<PipelineIndex, PipelineMetadata> m_pipelineMetadatas;

	std::vector<std::pair<Pipeline, uint8_t>> m_retiredPipelines;

	std::unordered_map<std::filesystem::path, std::unordered_set<PipelineIndex>>
		m_modules;

	uint32_t m_pipelineCount = 1;

	std::mutex m_mutex;
	std::condition_variable m_cv;
	bool m_monitorEnabled = false;
	std::thread m_monitorThread;

	std::unordered_map<std::filesystem::path, std::filesystem::file_time_type>
		m_lastEdited;

	void _monitor();

	std::optional<vk::ShaderModule> loadModule(
		const std::filesystem::path& path, SlangStage stage
	);

	std::optional<Pipeline> buildPipeline(const PipelineMetadata& metadata);

	void reloadPipeline(PipelineIndex index);

	static std::unordered_map<std::string_view, PipelineMetadata>
	_get_defined_pipelines();

public:
	ShaderEngine();
	~ShaderEngine();

	PipelineIndex registerPipeline(const PipelineMetadata metadata);

	Pipeline getPipeline(PipelineIndex index) {
		assert(m_pipelines.contains(index));
		return m_pipelines[index];
	}

	void flushRetiredPipelines();
};
