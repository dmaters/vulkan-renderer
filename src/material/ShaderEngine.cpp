#include "ShaderEngine.hpp"

#include <slang/slang.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "Instance.hpp"
#include "Pipeline.hpp"

ShaderEngine::ShaderEngine() : m_monitorThread(&ShaderEngine::_monitor, this) {
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
		.profile = m_session->findProfile("glsl_460"),

	};
	slang::ISession* session;

	const char* searchPaths[] = { "../resources/shaders" };

	std::vector<slang::CompilerOptionEntry> compilerOptions = {
		{
         .name = slang::CompilerOptionName::VulkanInvertY,
         .value = slang::CompilerOptionValue { .intValue0 = true },
		 }
	};

	m_session->createSession(
		{
			.structureSize = sizeof(slang::SessionDesc),
			.targets = &desc,
			.targetCount = 1,
			.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
			.searchPaths = searchPaths,
			.searchPathCount = 1,
			.compilerOptionEntries = compilerOptions.data(),
			.compilerOptionEntryCount = (uint32_t)compilerOptions.size(),
		},
		&session

	);

	slang::IBlob* diagnostics = nullptr;

	slang::IModule* slangModule =
		session->loadModule(path.string().c_str(), &diagnostics);

	if (diagnostics) {
		std::cerr << (char*)diagnostics->getBufferPointer() << std::endl;
		if (std::strstr((char*)diagnostics->getBufferPointer(), "error") !=
		    nullptr)
			return std::nullopt;
	}

	slang::IEntryPoint* entryPoint;
	slangModule->findEntryPointByName("main", &entryPoint);
	slang::IComponentType* components[] = { slangModule, entryPoint };
	slang::IComponentType* program;
	session->createCompositeComponentType(components, 2, &program);

	slang::IComponentType* linkedProgram;

	program->link(&linkedProgram, &diagnostics);

	if (diagnostics) {
		std::cerr << (char*)diagnostics->getBufferPointer() << std::endl;
		if (std::strstr((char*)diagnostics->getBufferPointer(), "error") !=
		    nullptr)
			return std::nullopt;
	}

	int entryPointIndex = 0;
	int targetIndex = 0;
	slang::IBlob* kernelBlob;

	linkedProgram->getEntryPointCode(
		entryPointIndex, targetIndex, &kernelBlob, &diagnostics
	);

	if (diagnostics) {
		std::cerr << (char*)diagnostics->getBufferPointer() << std::endl;
		if (std::strstr((char*)diagnostics->getBufferPointer(), "error") !=
		    nullptr)
			return std::nullopt;
	}

	vk::ShaderModule module = Instance::Get().device.createShaderModule({
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
		Instance::Get().device,
		{
			.shaderStages = modules,
			.setLayouts = metadata.layouts,
			.configuration = metadata.configuration,
		}
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

	if (!metadata.modules.vertex.empty()) {
		m_modules[metadata.modules.vertex].insert(index);
		m_lastEdited[metadata.modules.vertex] =
			std::filesystem::last_write_time(metadata.modules.vertex);
	}

	if (!metadata.modules.fragment.empty()) {
		m_modules[metadata.modules.fragment].insert(index);
		m_lastEdited[metadata.modules.fragment] =
			std::filesystem::last_write_time(metadata.modules.fragment);
	}

	return index;
}

void ShaderEngine::flushRetiredPipelines() {
	vk::Device& device = Instance::Get().device;

	std::lock_guard<std::mutex> lock(m_mutex);
	for (auto& [retiredPipeline, frameCount] : m_retiredPipelines) {
		frameCount--;
	}

	auto it = std::remove_if(
		m_retiredPipelines.begin(),
		m_retiredPipelines.end(),
		[&](const std::pair<Pipeline, uint8_t>& info) {
			if (info.second == 0) {
				device.destroyPipeline(info.first.pipeline);
				device.destroyPipelineLayout(info.first.pipelineLayout);

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

	while (m_monitorEnabled) {
		for (auto& [module, time] : m_lastEdited) {
			auto currentTime = std::filesystem::last_write_time(module);

			if (currentTime == time) continue;
			std::lock_guard<std::mutex> lock(m_mutex);

			for (PipelineIndex index : m_modules.at(module)) {
				PipelineMetadata& metadata = m_pipelineMetadatas.at(index);
				auto pipeline = buildPipeline(metadata);

				if (pipeline.has_value()) {
					m_retiredPipelines.push_back({ m_pipelines[index], 4 });
					m_pipelines[index] = pipeline.value();
				}
			}
			m_lastEdited[module] = currentTime;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}