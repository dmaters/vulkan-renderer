#pragma once

#include <array>
#include <chrono>
#include <cstdint>
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
	GraphData& m_data;
	ExecutionInfo m_execInfo;

	bool m_initialized = false;

	Swapchain& m_swapchain;
	ResourceManager& m_resourceManager;
	MaterialManager& m_materialManager;

	std::optional<ResourceManager::AllocationIndex> m_deviceAllocation;
	std::optional<ResourceManager::AllocationIndex> m_sharedAllocation;
	std::optional<ResourceManager::AllocationIndex> m_hostAllocation;

	std::unordered_map<ResourceIndex, ImageHandle> m_images;
	std::unordered_map<ResourceIndex, BufferHandle> m_buffers;

	std::array<vk::Semaphore, 3> m_renderSemaphores = {};

	vk::QueryPool m_debugQueryPool;
	std::chrono::time_point<std::chrono::steady_clock> m_beginFrameTimestamp;
	double m_lastCpuFrameTime;

	uint64_t m_currentFrame = 0;

	bool updateTimings() const;
	void outputToSwapchain(
		vk::CommandBuffer& commandBuffer, uint32_t imageIndex
	) const;

public:
	RenderGraphRunner(
		GraphData& data,
		Swapchain& swapchain,
		ResourceManager& resourceManager,
		MaterialManager& materialManager,
		ExecutionInfo execInfo
	);
	~RenderGraphRunner();

	bool submit(const Scene& scene);
};
}  // namespace rendergraph::internal
