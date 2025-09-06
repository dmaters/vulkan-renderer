#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "RenderGraphBuilder.hpp"
#include "Swapchain.hpp"
#include "material/MaterialManager.hpp"
#include "resources/Image.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"

class RenderGraph {
public:
private:
	struct Node;

	Swapchain& m_swapchain;
	ResourceManager& m_resourceManager;
	MaterialManager& m_materialManager;

	bool m_baseImagesInitialized = false;
	bool m_swapchainImagesInitialized = false;

	ResourceManager::AllocationIndex m_frameDataAllocation;
	ResourceManager::AllocationIndex m_resolutionDependentAllocation = 0;
	ResourceManager::AllocationIndex m_oldResolutionDependentAllocation = 0;
	uint8_t m_swapchainFlushCounter = 0;

	GraphData m_data;
	std::unordered_map<ResourceIndex, ImageHandle> m_images;
	std::unordered_map<ResourceIndex, BufferHandle> m_buffers;

	uint8_t m_currentFrame = 0;
	std::array<vk::Semaphore, 3> m_renderSemaphores = {};

	void writeMemoryBarrier(
		vk::CommandBuffer& commandBuffer, GraphData::TaskData& task
	) const;

	void initializeImages(vk::CommandBuffer& commandBuffer);

	void outputToSwapchain(vk::CommandBuffer& commandBuffer, uint32_t index);
	void clearUnusedResources();
	void buildSwapchainResources();
	void rebuildSwapchain();

public:
	RenderGraph(
		Swapchain& swapchain,
		ResourceManager& resourceManager,
		MaterialManager& materialManager
	);

	void submit(const Scene& scene);
	void build(GraphData buildData);
};
