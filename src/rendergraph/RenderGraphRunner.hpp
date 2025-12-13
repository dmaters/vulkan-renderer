#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "RenderGraphBuilder.hpp"
#include "Swapchain.hpp"
#include "material/MaterialManager.hpp"
#include "rendergraph/GraphData.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"
namespace rendergraph::internal {

class RenderGraphRunner {
private:
	const GraphData& m_data;

	Swapchain& m_swapchain;
	ResourceManager& m_resourceManager;
	MaterialManager& m_materialManager;

	bool m_baseImagesInitialized = false;
	bool m_swapchainImagesInitialized = false;

	ResourceManager::AllocationIndex m_frameDataAllocation;
	ResourceManager::AllocationIndex m_resolutionDependentAllocation = 0;
	ResourceManager::AllocationIndex m_oldResolutionDependentAllocation = 0;
	uint8_t m_swapchainFlushCounter = 0;

	std::unordered_map<ResourceIndex, ImageHandle> m_images;
	std::unordered_map<ResourceIndex, BufferHandle> m_buffers;

	std::unordered_map<FeatureIndex, bool> m_features;

	uint8_t m_currentFrame = 0;
	std::array<vk::Semaphore, 3> m_renderSemaphores = {};

	void build();
	void writeMemoryBarrier(
		vk::CommandBuffer& commandBuffer, GraphData::TaskData& task
	) const;

	void initializeImages(vk::CommandBuffer& commandBuffer);

	void outputToSwapchain(
		vk::CommandBuffer& commandBuffer, ResourceIndex output, uint32_t index
	);
	void clearUnusedResources();
	void buildSwapchainResources();
	void rebuildSwapchain();

public:
	RenderGraphRunner(
		const GraphData& data,
		Swapchain& swapchain,
		ResourceManager& resourceManager,
		MaterialManager& materialManager
	);

	void submit(
		const Scene& scene, ResourceIndex output, ExecutionInfo& execInfo
	);
};
}  // namespace rendergraph::internal
