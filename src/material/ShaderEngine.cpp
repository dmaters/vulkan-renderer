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
static constexpr std::string_view SHADER_PATH = "../resources/shaders/";
ShaderEngine::ShaderEngine(
	vk::Device& device, vk::DescriptorSetLayout globalLayout
) :
	m_device(device),
	m_globalLayout(globalLayout),
	m_monitorThread(&ShaderEngine::_monitor, this) {
	slang::createGlobalSession(&m_session);

	createPipelines();

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

	const char* searchPaths[] = { SHADER_PATH.data() };

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

void ShaderEngine::getPipelineLayouts(
	PipelineMetadata& metadata,
	vk::DescriptorSetLayout& materialLayout,
	vk::DescriptorSetLayout& instanceLayout
) {
	vk::DescriptorSetLayoutCreateInfo materialLayoutInfo {
		.bindingCount = (uint32_t)metadata.materialResources.size(),
		.pBindings = metadata.materialResources.data(),
	};
	materialLayout = m_device.createDescriptorSetLayout(materialLayoutInfo);

	vk::DescriptorSetLayoutCreateInfo instanceLayoutInfo {
		.bindingCount = (uint32_t)metadata.instanceResources.size(),
		.pBindings = metadata.instanceResources.data(),
	};
	instanceLayout = m_device.createDescriptorSetLayout(instanceLayoutInfo);
}

std::optional<Pipeline> ShaderEngine::createPipeline(
	const PipelineMetadata& metadata
) {
	std::vector<vk::PipelineShaderStageCreateInfo> modules;

	if (!metadata.modules.vertex.empty()) {
		auto res = loadModule(
			std::filesystem::path(SHADER_PATH) / metadata.modules.vertex
		);
		if (!res.has_value()) return std::nullopt;

		modules.push_back({
			.stage = vk::ShaderStageFlagBits::eVertex,
			.module = res.value(),
			.pName = "main",
		});
	}

	if (!metadata.modules.fragment.empty()) {
		auto res = loadModule(
			std::filesystem::path(SHADER_PATH) / metadata.modules.fragment
		);
		if (!res.has_value()) return std::nullopt;

		modules.push_back({
			.stage = vk::ShaderStageFlagBits::eFragment,
			.module = res.value(),
			.pName = "main",
		});
	}

	std::optional<Pipeline> pipeline = PipelineBuilder::BuildPipeline(
		m_device,
		{
			.shaderStages = modules,
			.globalSetLayout = m_globalLayout,
			.pipelineSetLayout = metadata.layouts.materialSetLayout,
			.instanceSetLayout = metadata.layouts.instanceSetLayout,
		}
	);

	return pipeline;
}

void ShaderEngine::createPipelines() {
	for (auto& [name, metadata] : _get_defined_pipelines()) {
		PipelineIndex index = m_pipelineCount;
		m_pipelineCount++;
		vk::DescriptorSetLayout materialLayout;
		vk::DescriptorSetLayout instanceLayout;

		getPipelineLayouts(metadata, materialLayout, instanceLayout);
		metadata.layouts = {
			.materialSetLayout = materialLayout,
			.instanceSetLayout = instanceLayout,
		};
		auto pipeline = createPipeline(metadata);

		if (name == "fallback_error") m_pipelines[0] = pipeline.value();

		if (!pipeline.has_value())
			m_brokenPipelines.insert(index);
		else
			m_pipelines[index] = pipeline.value();

		m_pipelineMetadatas[index] = metadata;

		m_names[name] = index;

		if (!metadata.modules.vertex.empty())
			m_modules
				[std::filesystem::path(SHADER_PATH) / metadata.modules.vertex]
					.insert(index);

		if (!metadata.modules.fragment.empty())
			m_modules
				[std::filesystem::path(SHADER_PATH) / metadata.modules.fragment]
					.insert(index);
	}
}

std::vector<PipelineIndex> ShaderEngine::getUpdatedPipelines() {
	std::vector<std::pair<PipelineIndex, std::optional<Pipeline>>>
		modifiedPipelines;

	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if (m_modifiedPipelines.empty()) return {};

		modifiedPipelines = m_modifiedPipelines.front();
		m_modifiedPipelines.pop();
	}

	std::vector<PipelineIndex> modifiedIndices;

	for (auto [index, pipeline] : modifiedPipelines) {
		if (!m_brokenPipelines.contains(index)) {
			Pipeline oldPipeline = m_pipelines[index];
			m_retiredPipelines.push_back({ oldPipeline, 4 });
		}

		if (!pipeline.has_value()) {
			m_brokenPipelines.insert(index);
			m_pipelines.erase(index);
		} else {
			if (m_brokenPipelines.contains(index))
				m_brokenPipelines.erase(index);

			m_pipelines[index] = pipeline.value();
		}

		modifiedIndices.push_back(index);
	}
	return modifiedIndices;
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

			std::vector<std::pair<PipelineIndex, std::optional<Pipeline>>>
				modifiedPipelines;
			for (auto& index : m_modules[module]) {
				const PipelineMetadata& metadata = getPipelineMetadata(index);
				auto pipeline = createPipeline(metadata);
				modifiedPipelines.push_back({ index, pipeline });
			}

			m_modifiedPipelines.push(modifiedPipelines);
			lastEdited[module] = currentTime;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}

std::unordered_map<std::string_view, PipelineMetadata>
ShaderEngine::_get_defined_pipelines() {
	return {
		{
         "fallback_error", {
			.modules = {
				.vertex= "fallback_error_vert.slang",
				.fragment = "fallback_error_frag.slang",
			},
			}, },
		{
         "pbr", { 
			.modules =
			{
				.vertex = "standard_forward_vert.slang",
		      	.fragment = "standard_forward_frag.slang",
		 	},
			.instanceResources = {vk::DescriptorSetLayoutBinding{
				.binding = 0,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.descriptorCount= 1,
				.stageFlags = vk::ShaderStageFlagBits::eFragment,
			},}, 
			},
		},
	};
}