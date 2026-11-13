#include "ShaderEngine.hpp"

#include <slang-com-ptr.h>
#include <slang.h>

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vulkan/vulkan.hpp>

#include "Instance.hpp"
#include "Pipeline.hpp"

slang::ISession* createSession(slang::IGlobalSession& globalSession) {
	const char* searchPaths[] = { "resources/shaders" };

	std::vector<slang::CompilerOptionEntry> compilerOptions = {
		{
		 .name = slang::CompilerOptionName::VulkanUseEntryPointName,
		 .value = slang::CompilerOptionValue { .intValue0 = true },
		 },
		{
		 .name = slang::CompilerOptionName::LanguageVersion,
		 .value = slang::CompilerOptionValue { .intValue0 = 2026 },
		 }
	};

	slang::TargetDesc desc {
		.format = SLANG_SPIRV,
		.profile = globalSession.findProfile("glsl_460"),
	};

	slang::ISession* session;
	globalSession.createSession(
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
	return session;
}

ShaderEngine::ShaderEngine() : m_monitorThread(&ShaderEngine::_monitor, this) {
	slang::createGlobalSession(&m_globalSession);

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_monitorEnabled = true;
	}
	m_cv.notify_all();

	m_registerSession = createSession(*m_globalSession);
}
ShaderEngine::~ShaderEngine() {
	m_registerSession->release();
	m_globalSession->release();

	m_monitorEnabled = false;
	m_monitorThread.join();
}

struct ModuleInfo {
	vk::ShaderModule module;
	std::vector<std::filesystem::path> moduleReferences;
};

std::optional<ModuleInfo> loadModule(
	slang::ISession& session, const std::filesystem::path& path, std::string_view entryPoint, SlangStage stage
) {
	std::cout << "Compiling :" << path.string() << std::endl;

	assert(!path.empty());

	slang::IBlob* diagnostics = nullptr;

	slang::IModule* slangModule;

	slangModule = session.loadModule(path.string().c_str(), &diagnostics);

	if (diagnostics) {
		std::cerr << (char*)diagnostics->getBufferPointer() << std::endl;
		if (std::strstr((char*)diagnostics->getBufferPointer(), "error") != nullptr) return std::nullopt;
	}

	Slang::ComPtr<slang::IEntryPoint> entryPointRef;

	slangModule->findAndCheckEntryPoint(entryPoint.data(), stage, entryPointRef.writeRef(), &diagnostics);

	if (diagnostics) {
		std::cerr << (char*)diagnostics->getBufferPointer() << std::endl;
		if (std::strstr((char*)diagnostics->getBufferPointer(), "error") != nullptr) return std::nullopt;
	}

	std::vector<std::filesystem::path> moduleReferences;
	moduleReferences.reserve(slangModule->getDependencyFileCount());
	for (int i = 0; i < slangModule->getDependencyFileCount(); i++) {
		moduleReferences.push_back(slangModule->getDependencyFilePath(i));
	}

	slang ::IComponentType* components[] = { slangModule, entryPointRef };
	slang::IComponentType* program;
	session.createCompositeComponentType(components, 2, &program, &diagnostics);

	if (diagnostics) {
		std::cerr << (char*)diagnostics->getBufferPointer() << std::endl;
		if (std::strstr((char*)diagnostics->getBufferPointer(), "error") != nullptr) return std::nullopt;
	}
	slang::IComponentType* linkedProgram;

	program->link(&linkedProgram, &diagnostics);
	if (diagnostics) {
		std::cerr << (char*)diagnostics->getBufferPointer() << std::endl;
		if (std::strstr((char*)diagnostics->getBufferPointer(), "error") != nullptr) return std::nullopt;
	}

	int entryPointIndex = 0;
	int targetIndex = 0;
	slang::IBlob* kernelBlob;

	linkedProgram->getEntryPointCode(entryPointIndex, targetIndex, &kernelBlob, &diagnostics);
	if (diagnostics) {
		std::cerr << (char*)diagnostics->getBufferPointer() << std::endl;
		if (std::strstr((char*)diagnostics->getBufferPointer(), "error") != nullptr) return std::nullopt;
	}

	slangModule->release();
	program->release();
	linkedProgram->release();

	vk::ShaderModule module = Instance::Get().device.createShaderModule(
		{
			.codeSize = kernelBlob->getBufferSize(),
			.pCode = (uint32_t*)kernelBlob->getBufferPointer(),
		}
	);
	kernelBlob->release();
	return ModuleInfo {
		.module = module,
		.moduleReferences = moduleReferences,
	};
}

struct BuildInfo {
	Pipeline pipeline;
	std::vector<std::filesystem::path> pipelineDependencies;
};

std::optional<BuildInfo> buildComputePipeline(
	ShaderModule module, std::array<vk::DescriptorSetLayout, 2>& layouts, slang::ISession& session
) {
	auto moduleInfo = loadModule(session, module.path, module.entryPoint, SlangStage::SLANG_STAGE_COMPUTE);
	if (!moduleInfo.has_value()) return std::nullopt;

	vk::PipelineShaderStageCreateInfo shaderModule = {
		.stage = vk::ShaderStageFlagBits::eCompute,
		.module = moduleInfo->module,
		.pName = module.entryPoint.data(),
	};

	auto pipeline = PipelineBuilder::BuildComputePipeline(
		{
			.stage = shaderModule,
			.setLayouts = layouts,
		}
	);

	if (!pipeline) return std::nullopt;

	return BuildInfo {
		.pipeline = *pipeline,
		.pipelineDependencies = moduleInfo->moduleReferences,
	};
}

std::optional<BuildInfo> buildGraphicPipeline(
	GraphicPipelineModules& modules,
	std::array<vk::DescriptorSetLayout, 2>& layouts,
	GraphicPipelineConfiguration& configuration,
	slang::ISession& session
) {
	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
	assert(!modules.vertex.path.empty());
	assert(!modules.fragment.path.empty());

	auto vertexModuleInfo =
		loadModule(session, modules.vertex.path, modules.vertex.entryPoint, SlangStage::SLANG_STAGE_VERTEX);
	if (!vertexModuleInfo.has_value()) return std::nullopt;

	shaderStages.push_back(
		{
			.stage = vk::ShaderStageFlagBits::eVertex,
			.module = vertexModuleInfo->module,
			.pName = modules.vertex.entryPoint.data(),
		}
	);

	auto fragmentModuleInfo =
		loadModule(session, modules.fragment.path, modules.fragment.entryPoint, SlangStage::SLANG_STAGE_FRAGMENT);
	if (!fragmentModuleInfo.has_value()) return std::nullopt;

	shaderStages.push_back(
		{
			.stage = vk::ShaderStageFlagBits::eFragment,
			.module = fragmentModuleInfo->module,
			.pName = modules.fragment.entryPoint.data(),
		}
	);
	auto pipeline = PipelineBuilder::BuildGraphicPipeline(
		{
			.shaderStages = shaderStages,
			.setLayouts = layouts,
			.configuration = configuration,
		}
	);
	if (!pipeline) return std::nullopt;
	std::vector<std::filesystem::path> pipelineDependencies;

	pipelineDependencies.insert(
		pipelineDependencies.end(), vertexModuleInfo->moduleReferences.begin(), vertexModuleInfo->moduleReferences.end()
	);
	pipelineDependencies.insert(
		pipelineDependencies.end(),
		fragmentModuleInfo->moduleReferences.begin(),
		fragmentModuleInfo->moduleReferences.end()
	);

	return BuildInfo {
		.pipeline = *pipeline,
		.pipelineDependencies = std::move(pipelineDependencies),
	};
}
std::optional<BuildInfo> buildRaytracingPipeline(
	RaytracingPipelineModules& shaderModules, std::array<vk::DescriptorSetLayout, 2> layouts, slang::ISession& session
) {
	auto raygenModuleInfo = loadModule(
		session, shaderModules.raygen.path, shaderModules.raygen.entryPoint, SlangStage::SLANG_STAGE_RAY_GENERATION
	);
	if (!raygenModuleInfo.has_value()) return std::nullopt;

	auto missModuleInfo =
		loadModule(session, shaderModules.miss.path, shaderModules.miss.entryPoint, SlangStage::SLANG_STAGE_MISS);
	if (!missModuleInfo.has_value()) return std::nullopt;

	auto closestHitModuleInfo = loadModule(
		session, shaderModules.closestHit.path, shaderModules.closestHit.entryPoint, SlangStage::SLANG_STAGE_CLOSEST_HIT
	);
	if (!closestHitModuleInfo.has_value()) return std::nullopt;

	std::array<vk::PipelineShaderStageCreateInfo, 3> modulesInfo {
		{
		 {
				.stage = vk::ShaderStageFlagBits::eRaygenKHR,
				.module = raygenModuleInfo->module,
				.pName = shaderModules.raygen.entryPoint.data(),
			}, {
				.stage = vk::ShaderStageFlagBits::eMissKHR,
				.module = missModuleInfo->module,
				.pName = shaderModules.miss.entryPoint.data(),
			}, {
				.stage = vk::ShaderStageFlagBits::eClosestHitKHR,
				.module = closestHitModuleInfo->module,
				.pName = shaderModules.closestHit.entryPoint.data(),
			}, },
	};

	std::array<vk::RayTracingShaderGroupCreateInfoKHR, 4> groups {
		{
			{
				.type = vk::RayTracingShaderGroupTypeKHR::eGeneral,	 // Raygen
				.generalShader = 0,
			}, {
				.type = vk::RayTracingShaderGroupTypeKHR::eGeneral,	 // Miss
				.generalShader = 1,
			}, {
				.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,  // Hit
				.generalShader = 2,
			}, {
				.type = vk::RayTracingShaderGroupTypeKHR::eGeneral,	 // Dummy
			}, },
	};

	auto pipeline = PipelineBuilder::BuildRaytracingPipeline(
		{
			.shaderStages = modulesInfo,
			.groups = groups,
			.setLayouts = layouts,
		}
	);

	if (!pipeline) return std::nullopt;

	std::vector<std::filesystem::path> dependencies;

	dependencies.insert(
		dependencies.end(), raygenModuleInfo->moduleReferences.begin(), raygenModuleInfo->moduleReferences.end()
	);
	dependencies.insert(
		dependencies.end(), closestHitModuleInfo->moduleReferences.begin(), closestHitModuleInfo->moduleReferences.end()
	);
	dependencies.insert(
		dependencies.end(), missModuleInfo->moduleReferences.begin(), missModuleInfo->moduleReferences.end()
	);

	return BuildInfo {
		.pipeline = *pipeline,
		.pipelineDependencies = dependencies,
	};
}

PipelineIndex ShaderEngine::registerComputePipeline(
	ShaderModule module, std::array<vk::DescriptorSetLayout, 2> layouts
) {
	assert(!module.path.empty());
	assert(std::filesystem::exists(module.path));

	PipelineIndex index = m_pipelines.size();
	m_pipelineTypes[index] = PipelineType::Compute;

	auto buildInfo = buildComputePipeline(module, layouts, *m_registerSession);
	assert(buildInfo.has_value());

	m_pipelines[index] = buildInfo->pipeline;
	m_layouts[index] = layouts;
	m_modules[index].computeModule = module;

	for (auto dependency : buildInfo->pipelineDependencies) {
		m_dependencies[dependency].insert(index);
		m_lastEdited[dependency] = std::filesystem::last_write_time(dependency);
	}

	return index;
}

PipelineIndex ShaderEngine::registerGraphicPipeline(
	GraphicPipelineModules modules,
	std::array<vk::DescriptorSetLayout, 2> layouts,
	GraphicPipelineConfiguration renderPassConfig
) {
	assert(!modules.vertex.path.empty() && !modules.vertex.path.empty());
	assert(std::filesystem::exists(modules.vertex.path) && std::filesystem::exists(modules.vertex.path));

	PipelineIndex index = m_pipelines.size();
	m_pipelineTypes[index] = PipelineType::Graphic;

	auto buildInfo = buildGraphicPipeline(modules, layouts, renderPassConfig, *m_registerSession);

	assert(buildInfo.has_value());

	m_pipelines[index] = buildInfo->pipeline;
	m_layouts[index] = layouts;
	m_modules[index].graphicsModules = modules;
	m_renderPassConfigurations[index] = renderPassConfig;

	for (auto dependency : buildInfo->pipelineDependencies) {
		m_dependencies[dependency].insert(index);
		m_lastEdited[dependency] = std::filesystem::last_write_time(dependency);
	}

	return index;
}

PipelineIndex ShaderEngine::registerRaytracingPipeline(
	RaytracingPipelineModules modules, std::array<vk::DescriptorSetLayout, 2> layouts
) {
	assert(!modules.raygen.path.empty() && !modules.closestHit.path.empty() && !modules.miss.path.empty());
	assert(
		std::filesystem::exists(modules.raygen.path) && std::filesystem::exists(modules.closestHit.path) &&
		std::filesystem::exists(modules.miss.path)
	);

	PipelineIndex index = m_pipelines.size();
	m_pipelineTypes[index] = PipelineType::Raytracing;

	auto buildInfo = buildRaytracingPipeline(modules, layouts, *m_registerSession);

	assert(buildInfo.has_value());

	m_pipelines[index] = buildInfo->pipeline;
	m_layouts[index] = layouts;
	m_modules[index].raytracingModules = modules;

	for (auto dependency : buildInfo->pipelineDependencies) {
		m_dependencies[dependency].insert(index);
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
		m_retiredPipelines.begin(), m_retiredPipelines.end(), [&](const std::pair<Pipeline, uint8_t>& info) {
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
		auto session = createSession(*m_globalSession);
		for (auto& [module, registeredTime] : m_lastEdited) {
			auto lastEditedTime = std::filesystem::last_write_time(module);

			if (lastEditedTime == registeredTime) continue;

			std::lock_guard<std::mutex> lock(m_mutex);

			slang::IBlob* diagnostics = nullptr;
			session->loadModule(module.string().c_str(), &diagnostics);

			if (diagnostics) {
				std::cerr << (char*)diagnostics->getBufferPointer() << std::endl;
				if (std::strstr((char*)diagnostics->getBufferPointer(), "error") != nullptr) continue;
			}

			for (PipelineIndex index : m_dependencies.at(module)) {
				PipelineType type = m_pipelineTypes[index];

				std::optional<BuildInfo> buildInfo;

				auto& layouts = m_layouts[index];

				if (type == PipelineType::Graphic) {
					auto& modules = m_modules[index].graphicsModules;
					auto& renderPass = m_renderPassConfigurations[index];
					buildInfo = buildGraphicPipeline(modules, layouts, renderPass, *session);
				} else if (type == PipelineType::Compute) {
					auto& module = m_modules[index].computeModule;
					buildInfo = buildComputePipeline(module, layouts, *session);
				} else if (type == PipelineType::Raytracing) {
					auto& module = m_modules[index].raytracingModules;
					buildInfo = buildRaytracingPipeline(module, layouts, *session);
				}

				if (!buildInfo.has_value()) continue;

				m_retiredPipelines.push_back({ m_pipelines[index], 4 });
				m_pipelines[index] = buildInfo->pipeline;

				for (auto dependency : buildInfo->pipelineDependencies) {
					m_dependencies[dependency].insert(index);
					m_lastEdited[dependency] = std::filesystem::last_write_time(dependency);
				}
			}
			registeredTime = lastEditedTime;
		}
		session->release();
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}
