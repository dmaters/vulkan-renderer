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
#include "material/MaterialManager.hpp"
#include "resources/ResourceManager.hpp"
#include "tasks/Task.hpp"

struct Primitive;
struct Resources {
	ResourceManager& resourceManager;
	MaterialManager& materialManager;
	const std::vector<Primitive>& primitives;
	uint8_t currentFrame;
};

class RenderGraph {
public:
private:
	struct Node;
	struct RegisteredTask;

	Instance& m_instance;
	Swapchain& m_swapchain;
	ResourceManager& m_resourceManager;

	std::vector<RegisteredTask> m_registeredTask;
	std::vector<uint16_t> m_nodes;

	std::unordered_map<std::string_view, ResourceManager::BufferDescription>
		m_bufferCreationsInfos;
	std::unordered_map<std::string_view, ResourceManager::ImageDescription>
		m_imageCreationInfos;
	std::unordered_map<std::string_view, uint8_t> m_swapchainDependentImages;

	std::unordered_map<std::string_view, ImageHandle> m_images;
	std::unordered_map<std::string_view, ImageHandle> m_buffers;

	std::array<std::vector<ImageHandle>, 3> m_unusedImages;
	std::array<std::vector<BufferHandle>, 3> m_unusedBuffers;
	uint8_t m_swapchainFlushCounter = 0;

	vk::Queue m_graphicQueue;

	RenderGraphBuilder m_builder;

	uint8_t m_currentFrame = 0;
	bool m_initialized = false;

	bool addImageBarrier(
		ImageDependencyInfo& image, vk::ImageMemoryBarrier2& buffer
	);
	void addMemoryBarriers(
		vk::CommandBuffer& commandBuffer, std::string_view task
	);

	void buildGraph();
	void initializeExternalImages(vk::CommandBuffer& commandBuffer);

	void outputToSwapchain(vk::CommandBuffer& commandBuffer, uint32_t index);
	void clearUnusedResources();

	void rebuildSwapchain();

public:
	RenderGraph(
		Instance& instance,
		Swapchain& swapchain,
		ResourceManager& resourceManager
	);

	void addImage(
		std::string_view name,
		ResourceManager::ImageDescription description,
		uint8_t swapchainResolutionMultiplier
	);

	void addBuffer(
		std::string_view name, ResourceManager::BufferDescription description
	);

	void submit(const std::vector<Primitive>& primitives);
	void build(GraphData buildData);
};

struct RenderGraph::RegisteredTask {
	std::unique_ptr<Task> task;
	Barriers barriers;
	std::unordered_map<std::string_view, ImageDependencyInfo> images;
	std::unordered_map<std::string_view, BufferDependencyInfo> buffers;
};