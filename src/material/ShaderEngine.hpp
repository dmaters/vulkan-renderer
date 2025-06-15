#pragma once

#include <slang/slang.h>

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <queue>
#include <semaphore>
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
	vk::Device& m_device;

	slang::IGlobalSession* m_session;

	std::unordered_map<PipelineIndex, Pipeline> m_pipelines;

	std::unordered_map<std::filesystem::path, std::unordered_set<PipelineIndex>>
		m_modules;

	uint32_t m_pipelineCount = 0;

	std::vector<std::pair<Pipeline, uint8_t>> m_retiredPipelines;

	std::mutex m_mutex;
	std::condition_variable m_cv;
	bool m_monitorEnabled = false;
	std::thread m_monitorThread;
	void _monitor();

	std::optional<vk::ShaderModule> loadModule(const std::filesystem::path& path
	);

	void getPipelineLayouts(
		PipelineMetadata& metadata,
		vk::DescriptorSetLayout& materialLayout,
		vk::DescriptorSetLayout& instanceLayout
	);

	std::optional<Pipeline> createPipeline(const PipelineMetadata& metadata);

	void reloadPipeline(PipelineIndex index);

	static std::unordered_map<std::string_view, PipelineMetadata>
	_get_defined_pipelines();

public:
	ShaderEngine(vk::Device& device, vk::DescriptorSetLayout globalLayout);
	~ShaderEngine();

	PipelineIndex registerPipeline(const PipelineMetadata& metadata);

	std::optional<Pipeline> getPipeline(PipelineIndex index) {
		if (m_brokenPipelines.contains(index)) return std::nullopt;
		assert(m_pipelines.contains(index));
		return m_pipelines[index];
	}

	void flushRetiredPipelines();
};
