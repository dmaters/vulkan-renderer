#include "ShaderEngine.hpp"

#include <slang-com-ptr.h>
#include <slang.h>

#include <bit>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <regex>
#include <string>
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
	const std::filesystem::path& path,
	std::string_view entryPoint,
	SlangStage stage
) {
	std::cout << "Compiling :" << path.string() << std::endl;
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

	Slang::ComPtr<slang::IEntryPoint> entryPointRef;

	slangModule->findAndCheckEntryPoint(
		entryPoint.data(), stage, entryPointRef.writeRef(), &diagnostics
	);

	if (diagnostics) {
		std::cerr << (char*)diagnostics->getBufferPointer() << std::endl;
		if (std::strstr((char*)diagnostics->getBufferPointer(), "error") !=
		    nullptr)
			return std::nullopt;
	}

	slang ::IComponentType* components[] = { slangModule, entryPointRef };
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

	vk::ShaderModule module = Instance::Get().device.createShaderModule(
		{
			.codeSize = kernelBlob->getBufferSize(),
			.pCode = (uint32_t*)kernelBlob->getBufferPointer(),
		}
	);

	session->release();

	return module;
}
std::optional<Pipeline> ShaderEngine::buildComputePipeline(
	ShaderModule module, std::vector<vk::DescriptorSetLayout>& layouts
) {
	assert(!module.path.empty());
	auto res = loadModule(
		module.path, module.entryPoint, SlangStage::SLANG_STAGE_COMPUTE
	);
	if (!res.has_value()) return std::nullopt;

	vk::PipelineShaderStageCreateInfo shaderModule = {
		.stage = vk::ShaderStageFlagBits::eCompute,
		.module = res.value(),
		.pName = "main",
	};

	return PipelineBuilder::BuildComputePipeline(
		{
			.stage = shaderModule,
			.setLayouts = layouts,
		}
	);
}

std::optional<Pipeline> ShaderEngine::buildGraphicPipeline(
	GraphicPipelineModules& modules,
	std::vector<vk::DescriptorSetLayout>& layouts,
	GraphicPipelineConfiguration& configuration
) {
	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
	assert(!modules.vertex.path.empty());
	assert(!modules.fragment.path.empty());

	auto vertexModule = loadModule(
		modules.vertex.path,
		modules.vertex.entryPoint,
		SlangStage::SLANG_STAGE_VERTEX
	);
	if (!vertexModule.has_value()) return std::nullopt;

	shaderStages.push_back(
		{
			.stage = vk::ShaderStageFlagBits::eVertex,
			.module = vertexModule.value(),
			.pName = modules.vertex.entryPoint.data(),
		}
	);

	auto fragmentModule = loadModule(
		modules.fragment.path,
		modules.fragment.entryPoint,
		SlangStage::SLANG_STAGE_FRAGMENT
	);
	if (!fragmentModule.has_value()) return std::nullopt;

	shaderStages.push_back(
		{
			.stage = vk::ShaderStageFlagBits::eFragment,
			.module = fragmentModule.value(),
			.pName = modules.fragment.entryPoint.data(),
		}
	);

	if (!modules.geometry.path.empty()) {
		auto geometryValue = loadModule(
			modules.geometry.path,
			modules.geometry.entryPoint,
			SlangStage::SLANG_STAGE_GEOMETRY
		);
		if (!geometryValue.has_value()) return std::nullopt;
		shaderStages.push_back(
			{
				.stage = vk::ShaderStageFlagBits::eGeometry,
				.module = geometryValue.value(),
				.pName = modules.geometry.entryPoint.data(),
			}
		);
	}

	return PipelineBuilder::BuildGraphicPipeline(
		{
			.shaderStages = shaderStages,
			.setLayouts = layouts,
			.configuration = configuration,
		}
	);
}
void getDependencies(
	std::filesystem::path base, std::unordered_set<std::filesystem::path>& paths
) {
	if (paths.contains(base)) return;
	assert(std::filesystem::exists(base));

	std::ifstream file(base);

	paths.insert(base);

	std::string line;
	std::regex pattern1(R"(#include\s*\"([^\"]+)\")");
	std::regex pattern2(R"((__include|import)\s*([A-Za-z0-9._]+;))");

	std::smatch match;

	std::unordered_set<std::filesystem::path> result;

	result.insert(base);

	static const std::filesystem::path _rootPath("resources/shaders");

	while (std::getline(file, line)) {
		if (std::regex_search(line, match, pattern1)) {
			std::filesystem::path path(match[1].str());
			if (std::filesystem::exists(base.parent_path() / path))
				getDependencies(base.parent_path() / path, paths);
			else if (std::filesystem::exists(_rootPath / path))
				getDependencies(_rootPath / path, paths);
			else
				throw "Module " + path.string() + " not found.";
		}
		if (std::regex_search(line, match, pattern2)) {
			std::string import = match[2].str();
			std::replace(import.begin(), import.end(), '.', '/');
			import.pop_back();
			import.append(".slang");

			std::filesystem::path path = std::filesystem::path(import);
			if (std::filesystem::exists(base.parent_path() / path))
				getDependencies(base.parent_path() / path, paths);
			else if (std::filesystem::exists(_rootPath / path))
				getDependencies(_rootPath / path, paths);
			else
				throw "Module " + path.string() + " not found.";
		}
	}

	file.close();
}

PipelineIndex ShaderEngine::registerComputePipeline(
	ShaderModule module, std::vector<vk::DescriptorSetLayout> layouts
) {
	PipelineIndexFields fields {
		.type = static_cast<uint8_t>(PipelineIndexFields::Type::Compute),
		.index = static_cast<uint32_t>(m_pipelines.size()),
	};
	PipelineIndex index = std::bit_cast<PipelineIndex>(fields);

	auto pipeline = buildComputePipeline(module, layouts);

	assert(pipeline.has_value());

	m_pipelines[index] = pipeline.value();
	m_layouts[index] = layouts;
	m_computeModules[index] = module;

	std::unordered_set<std::filesystem::path> dependencies;

	m_modules[module.path].insert(index);
	m_lastEdited[module.path] = std::filesystem::last_write_time(module.path);

	getDependencies(module.path, dependencies);

	for (auto dependecy : dependencies) {
		m_modules[dependecy].insert(index);
		m_lastEdited[dependecy] = std::filesystem::last_write_time(dependecy);
	}

	return index;
}

PipelineIndex ShaderEngine::registerGraphicPipeline(
	GraphicPipelineModules modules,
	std::vector<vk::DescriptorSetLayout> layouts,
	GraphicPipelineConfiguration renderPassConfig
) {
	PipelineIndexFields fields {
		.type = static_cast<uint8_t>(PipelineIndexFields::Type::Graphic),
		.index = static_cast<uint32_t>(m_pipelines.size()),
	};
	PipelineIndex index = std::bit_cast<PipelineIndex>(fields);

	auto pipeline = buildGraphicPipeline(modules, layouts, renderPassConfig);

	assert(pipeline.has_value());

	m_pipelines[index] = pipeline.value();
	m_layouts[index] = layouts;
	m_graphicModules[index] = modules;
	m_renderPassConfigurations[index] = renderPassConfig;

	std::unordered_set<std::filesystem::path> dependencies;

	m_modules[modules.vertex.path].insert(index);
	m_lastEdited[modules.vertex.path] =
		std::filesystem::last_write_time(modules.vertex.path);

	getDependencies(modules.vertex.path, dependencies);

	if (!modules.geometry.path.empty()) {
		m_modules[modules.geometry.path].insert(index);
		m_lastEdited[modules.geometry.path] =
			std::filesystem::last_write_time(modules.geometry.path);

		getDependencies(modules.geometry.path, dependencies);
	}

	m_modules[modules.fragment.path].insert(index);
	m_lastEdited[modules.fragment.path] =
		std::filesystem::last_write_time(modules.fragment.path);

	getDependencies(modules.fragment.path, dependencies);

	for (auto dependency : dependencies) {
		m_modules[dependency].insert(index);
		m_lastEdited[dependency] = std::filesystem::last_write_time(dependency);
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
				PipelineIndexFields::Type type =
					static_cast<PipelineIndexFields::Type>(
						std::bit_cast<PipelineIndexFields>(index).type
					);

				std::optional<Pipeline> pipeline;

				if (type == PipelineIndexFields::Type::Graphic) {
					auto& modules = m_graphicModules[index];
					auto& layouts = m_layouts[index];
					auto& renderPass = m_renderPassConfigurations[index];
					pipeline =
						buildGraphicPipeline(modules, layouts, renderPass);
				} else if (type == PipelineIndexFields::Type::Compute) {
					auto& module = m_computeModules[index];
					auto& layouts = m_layouts[index];
					pipeline = buildComputePipeline(module, layouts);
				}

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
