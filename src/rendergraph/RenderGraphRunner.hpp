#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vulkan/vulkan.hpp>

#include "Swapchain.hpp"
#include "material/MaterialManager.hpp"
#include "rendergraph/GraphData.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"
namespace rendergraph::internal {

class RenderGraphRunner {
private:
	const GraphData& m_data;
	ExecutionInfo m_execInfo;
	ResourceIndex m_output;
	bool m_initialized = false;

	Swapchain& m_swapchain;
	ResourceManager& m_resourceManager;
	MaterialManager& m_materialManager;

	ResourceManager::DeviceAllocationIndex m_frameDataAllocation;
	ResourceManager::DeviceAllocationIndex m_sharedDataAllocation;
	std::optional<ResourceManager::HostAllocationIndex> m_hostDataAllocation;

	ResourceManager::DeviceAllocationIndex m_resolutionDependentAllocation = 0;
	ResourceManager::DeviceAllocationIndex m_oldResolutionDependentAllocation =
		0;

	uint8_t m_swapchainFlushCounter = 0;

	std::unordered_map<ResourceIndex, ImageHandle> m_images;
	std::unordered_map<ResourceIndex, BufferHandle> m_buffers;

	std::unordered_map<FeatureIndex, bool> m_features;

	uint64_t m_currentFrame = 0;
	std::array<vk::Semaphore, 3> m_renderSemaphores = {};

	vk::QueryPool m_debugQueryPool;
	std::chrono::time_point<std::chrono::steady_clock> m_beginFrameTimestamp;
	double m_lastCpuFrameTime;

	void build();

	void initializeImages(vk::CommandBuffer& commandBuffer);

	void outputToSwapchain(
		vk::CommandBuffer& commandBuffer, ResourceIndex output, uint32_t index
	);
	void clearUnusedResources();
	void buildSwapchainResources();
	void rebuildSwapchain();
	bool updateTimings() const;

public:
	RenderGraphRunner(
		const GraphData& data,
		Swapchain& swapchain,
		ResourceManager& resourceManager,
		MaterialManager& materialManager
	);
	void update(ResourceIndex output, ExecutionInfo execInfo);
	void submit(const Scene& scene);
};
}  // namespace rendergraph::internal
