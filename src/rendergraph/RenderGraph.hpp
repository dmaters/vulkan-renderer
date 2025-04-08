#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "RenderGraphBuilder.hpp"
#include "Swapchain.hpp"
#include "resources/ResourceManager.hpp"
#include "tasks/Task.hpp"

struct Primitive;
struct Resources {
	ResourceManager& resourceManager;
	const std::vector<Primitive>& primitives;
	uint8_t currentFrame;
};

class RenderGraph {
public:
private:
	struct Node;
	struct RegisteredTask;

	std::unordered_map<std::string_view, RegisteredTask> m_registeredTask;
	std::vector<std::string_view> m_nodes;
	std::set<std::string_view> m_internalResources;
	std::set<std::string_view> m_uninitializedResources;

	std::vector<vk::ImageMemoryBarrier2> m_initializationBarriers;
	bool m_initialized = false;

	std::array<ImageHandle, 3> m_swapchainImages;

	Instance& m_instance;
	Swapchain& m_swapchain;
	ResourceManager& m_resourceManager;
	vk::Queue m_mainQueue;

	RenderGraphBuilder m_builder;

	bool addImageBarrier(
		ImageDependencyInfo& image, vk::ImageMemoryBarrier2& buffer
	);
	void addMemoryBarriers(
		vk::CommandBuffer& commandBuffer, std::string_view task
	);

	void buildGraph();

	uint8_t m_currentFrame = 0;

public:
	RenderGraph(
		Instance& instance,
		Swapchain& swapchain,
		ResourceManager& resourceManager
	);

	void addImage(
		std::string_view name,
		const ResourceManager::ImageDescription& description
	);

	void addBuffer(
		std::string_view name,
		const ResourceManager::BufferDescription& description
	);

	void addTask(std::string_view name, std::unique_ptr<Task> task);
	void submit(const std::vector<Primitive>& primitives);
	void build();
};

struct RenderGraph::RegisteredTask {
	std::unique_ptr<Task> task;
	std::optional<std::array<Barriers, 3>> barriers;
	std::vector<ImageDependencyInfo> images;
	std::vector<BufferDependencyInfo> buffers;
};