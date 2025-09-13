#include "ShaderEngine.hpp"

#include <slang/slang-com-ptr.h>
#include <slang/slang.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vulkan/vulkan.hpp>

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
	const std::filesystem::path& path, SlangStage stage
) {
	assert(!path.empty());

	slang::TargetDesc desc {
		.format = SLANG_SPIRV,
		.profile = m_session->findProfile("glsl_460"),

	};
	slang::ISession* session;

	const char* searchPaths[] = { "resources/shaders" };

	std::vector<slang::CompilerOptionEntry> compilerOptions = {
		{
         .name = slang::CompilerOptionName::VulkanInvertY,
         .value = slang::CompilerOptionValue { .intValue0 = true },
		 },
		{
         .name = slang::CompilerOptionName::VulkanUseEntryPointName,
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

	slang::IModule* slangModule;

	slangModule = session->loadModule(path.string().c_str(), &diagnostics);

	if (diagnostics) {
		std::cerr << (char*)diagnostics->getBufferPointer() << std::endl;
		if (std::strstr((char*)diagnostics->getBufferPointer(), "error") !=
		    nullptr)
			return std::nullopt;
	}

	Slang::ComPtr<slang::IEntryPoint> entryPoint;

	slangModule->findAndCheckEntryPoint(
		"main", stage, entryPoint.writeRef(), &diagnostics
	);

	if (diagnostics) {
		std::cerr << (char*)diagnostics->getBufferPointer() << std::endl;
		if (std::strstr((char*)diagnostics->getBufferPointer(), "error") !=
		    nullptr)
			return std::nullopt;
	}

	slang ::IComponentType* components[] = { slangModule, entryPoint };
	slang::IComponentType* program;
	session->createCompositeComponentType(
		components, 2, &program, &diagnostics
	);

	if (diagnostics) {
		std::cerr << (char*)diagnostics->getBufferPointer() << std::endl;
		if (std::strstr((char*)diagnostics->getBufferPointer(), "error") !=
		    nullptr)
			return std::nullopt;
	}
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
	if (!metadata.modules.compute.empty()) {
		auto res = loadModule(
			metadata.modules.compute, SlangStage::SLANG_STAGE_COMPUTE
		);
		if (!res.has_value()) return std::nullopt;

		vk::PipelineShaderStageCreateInfo module = {
			.stage = vk::ShaderStageFlagBits::eCompute,
			.module = res.value(),
			.pName = "main",
		};

		return PipelineBuilder::BuildComputePipeline({
			.stage = module,
			.setLayouts = metadata.layouts,
		});
	}

	std::vector<vk::PipelineShaderStageCreateInfo> modules;

	if (!metadata.modules.vertex.empty()) {
		auto res =
			loadModule(metadata.modules.vertex, SlangStage::SLANG_STAGE_VERTEX);
		if (!res.has_value()) return std::nullopt;

		modules.push_back({
			.stage = vk::ShaderStageFlagBits::eVertex,
			.module = res.value(),
			.pName = "main",
		});
	}

	if (!metadata.modules.fragment.empty()) {
		auto res = loadModule(
			metadata.modules.fragment, SlangStage::SLANG_STAGE_FRAGMENT
		);
		if (!res.has_value()) return std::nullopt;

		modules.push_back({
			.stage = vk::ShaderStageFlagBits::eFragment,
			.module = res.value(),
			.pName = "main",
		});
	}

	return PipelineBuilder::BuildGraphicPipeline({
		.shaderStages = modules,
		.setLayouts = metadata.layouts,
		.configuration = metadata.configuration,
	});
}
std::unordered_set<std::filesystem::path> getModuleTree(
	std::filesystem::path base
) {
	std::ifstream file(base);
	if (!file) {
		return {};
	}

	std::string line;
	std::regex pattern(R"(#include\s*\"([^\"]+)\")");
	std::smatch match;

	std::unordered_set<std::filesystem::path> result;

	result.insert(base);

	while (std::getline(file, line)) {
		if (std::regex_search(line, match, pattern)) {
			auto modules = getModuleTree(
				base.parent_path() / std::filesystem::path(match[1].str())
			);

			for (auto module : modules) result.insert(module);
		}
	}

	file.close();

	return result;
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

		auto tree = getModuleTree(metadata.modules.vertex);

		for (auto module : tree) {
			m_modules[module].insert(index);
			m_lastEdited[module] = std::filesystem::last_write_time(module);
		}
	}

	if (!metadata.modules.fragment.empty()) {
		m_modules[metadata.modules.fragment].insert(index);
		m_lastEdited[metadata.modules.fragment] =
			std::filesystem::last_write_time(metadata.modules.fragment);

		auto tree = getModuleTree(metadata.modules.fragment);
		for (auto module : tree) {
			m_modules[module].insert(index);
			m_lastEdited[module] = std::filesystem::last_write_time(module);
		}
	}

	if (!metadata.modules.compute.empty()) {
		m_modules[metadata.modules.compute].insert(index);
		m_lastEdited[metadata.modules.compute] =
			std::filesystem::last_write_time(metadata.modules.compute);

		auto tree = getModuleTree(metadata.modules.compute);
		for (auto module : tree) {
			m_modules[module].insert(index);
			m_lastEdited[module] = std::filesystem::last_write_time(module);
		}
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
