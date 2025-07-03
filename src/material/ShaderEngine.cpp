#include "ShaderEngine.hpp"

#include <slang/slang.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Pipeline.hpp"

ShaderEngine::ShaderEngine(
	vk::Device& device, vk::DescriptorSetLayout globalLayout
) :
	m_device(device), m_monitorThread(&ShaderEngine::_monitor, this) {
	slang::createGlobalSession(&m_session);

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_monitorEnabled = true;
	}
	m_cv.notify_all();
}
ShaderEngine::~ShaderEngine() {
	m_monitorEnabled = false;
	m_monitorThread.join();
}
std::optional<vk::ShaderModule> ShaderEngine::loadModule(
	const std::filesystem::path& path
) {
	slang::TargetDesc desc {
		.format = SLANG_SPIRV,
		.profile = m_session->findProfile("glsl_450"),

	};
	slang::ISession* session;

	const char* searchPaths[] = { "resources/shaders/" };

	m_session->createSession(
		{
			.structureSize = sizeof(slang::SessionDesc),
			.targets = &desc,
			.targetCount = 1,
			.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
			.searchPaths = searchPaths,
			.searchPathCount = 1,

		},
		&session

	);

	slang::IBlob* diagnostics = nullptr;

	slang::IModule* slangModule =
		session->loadModule(path.string().c_str(), &diagnostics);

	if (diagnostics &&
	    std::strstr((char*)diagnostics->getBufferPointer(), "error") !=
	        nullptr) {
		std::cerr << (char*)diagnostics->getBufferPointer() << std::endl;
		return std::nullopt;
	}
	slang::IEntryPoint* entryPoint;
	slangModule->findEntryPointByName("main", &entryPoint);
	slang::IComponentType* components[] = { slangModule, entryPoint };
	slang::IComponentType* program;
	session->createCompositeComponentType(components, 2, &program);

	slang::IComponentType* linkedProgram;

	program->link(&linkedProgram, &diagnostics);

	if (diagnostics &&
	    std::strstr((char*)diagnostics->getBufferPointer(), "error") !=
	        nullptr) {
		std::cerr << (char*)diagnostics->getBufferPointer() << std::endl;
		return std::nullopt;
	}

	int entryPointIndex = 0;
	int targetIndex = 0;
	slang::IBlob* kernelBlob;

	linkedProgram->getEntryPointCode(
		entryPointIndex, targetIndex, &kernelBlob, &diagnostics
	);
	if (diagnostics &&
	    std::strstr((char*)diagnostics->getBufferPointer(), "error") !=
	        nullptr) {
		std::cerr << (char*)diagnostics->getBufferPointer() << std::endl;
		return std::nullopt;
	}

	vk::ShaderModule module = m_device.createShaderModule({
		.codeSize = kernelBlob->getBufferSize(),
		.pCode = (uint32_t*)kernelBlob->getBufferPointer(),
	});

	session->release();

	return module;
}

std::optional<Pipeline> ShaderEngine::buildPipeline(
	const PipelineMetadata& metadata
) {
	std::vector<vk::PipelineShaderStageCreateInfo> modules;

	if (!metadata.modules.vertex.empty()) {
		auto res = loadModule(metadata.modules.vertex);
		if (!res.has_value()) return std::nullopt;

		modules.push_back({
			.stage = vk::ShaderStageFlagBits::eVertex,
			.module = res.value(),
			.pName = "main",
		});
	}

	if (!metadata.modules.fragment.empty()) {
		auto res = loadModule(metadata.modules.fragment);
		if (!res.has_value()) return std::nullopt;

		modules.push_back({
			.stage = vk::ShaderStageFlagBits::eFragment,
			.module = res.value(),
			.pName = "main",
		});
	}

	std::optional<Pipeline> pipeline = PipelineBuilder::BuildPipeline(
		m_device, { .shaderStages = modules, .setLayouts = metadata.layouts }
	);

	return pipeline;
}

PipelineIndex ShaderEngine::registerPipeline(const PipelineMetadata metadata) {
	PipelineIndex index = m_pipelineCount;
	m_pipelineCount++;

	auto pipeline = buildPipeline(metadata);

	assert(pipeline.has_value());

	m_pipelines[index] = pipeline.value();
	m_pipelineMetadatas[index] = metadata;

	if (!metadata.modules.vertex.empty())
		m_modules[metadata.modules.vertex].insert(index);

	if (!metadata.modules.fragment.empty())
		m_modules[metadata.modules.fragment].insert(index);

	return index;
}

void ShaderEngine::flushRetiredPipelines() {
	std::lock_guard<std::mutex> lock(m_mutex);
	for (auto& [retiredPipeline, frameCount] : m_retiredPipelines) {
		frameCount--;
	}

	auto it = std::remove_if(
		m_retiredPipelines.begin(),
		m_retiredPipelines.end(),
		[&](const std::pair<Pipeline, uint8_t>& info) {
			if (info.second == 0) {
				m_device.destroyPipeline(info.first.pipeline);
				m_device.destroyPipelineLayout(info.first.pipelineLayout);

				return true;
			}
			return false;
		}
	);
	m_retiredPipelines.erase(it, m_retiredPipelines.end());
}

void ShaderEngine::_monitor() {
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cv.wait(lock, [this] { return m_monitorEnabled; });
	}

	std::unordered_map<std::filesystem::path, std::filesystem::file_time_type>
		lastEdited;

	for (auto& [module, _] : m_modules) {
		lastEdited[module] = std::filesystem::last_write_time(module);
	}

	while (m_monitorEnabled) {
		for (auto& [module, time] : lastEdited) {
			auto currentTime = std::filesystem::last_write_time(module);

			if (currentTime == time) continue;
			std::lock_guard<std::mutex> lock(m_mutex);

			for (auto& index : m_modules[module]) {
				PipelineMetadata metadata = m_pipelineMetadatas[index];
				auto pipeline = buildPipeline(metadata);

				if (pipeline.has_value())
					m_modifiedPipelines[index] = pipeline.value();
			}
			lastEdited[module] = currentTime;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}