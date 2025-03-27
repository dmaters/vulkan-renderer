#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <set>
#include <string_view>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#include "RenderGraphResourceSolver.hpp"
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

	std::map<std::string_view, std::vector<std::string_view>> m_imageReferences;
	std::map<std::string_view, std::vector<std::string_view>>
		m_bufferReferences;

	std::map<std::string_view, RegisteredTask> m_registeredTask;

	std::array<ImageHandle, 3> m_swapchainImages;

	std::vector<std::string_view> m_nodes;

	Instance& m_instance;
	Swapchain& m_swapchain;
	ResourceManager& m_resourceManager;
	vk::Queue m_mainQueue;

	void clearImage(vk::CommandBuffer& buffer, std::string_view image);

	bool addImageBarrier(
		RenderGraphResourceSolver::ImageDependencyInfo& image,
		vk::ImageMemoryBarrier2& buffer
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

	void addTask(std::string_view name, std::unique_ptr<Task> task);

	void submit(const std::vector<Primitive>& primitives);
};
struct ImageDependency {
	std::string_view name;
	vk::ImageLayout required;
};
struct BufferDependency {
	std::string_view name;
};
struct RenderGraph::Node {
	std::string_view task;
	std::vector<ImageDependency> images;
	std::vector<BufferDependency> buffers;
};

struct RenderGraph::RegisteredTask {
	std::unique_ptr<Task> task;
	std::vector<RenderGraphResourceSolver::ImageDependencyInfo> images;
	std::vector<RenderGraphResourceSolver::BufferDependencyInfo> buffers;
};