#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <unordered_map>
#include <vulkan/vulkan.hpp>

#include "RenderingConfiguration.hpp"
#include "Swapchain.hpp"
#include "TaskRegister.hpp"
#include "material/MaterialManager.hpp"
#include "rendergraph/GraphData.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"

namespace rendergraph::internal {

class RenderGraphRunner {
private:
	ExecutionInfo m_execInfo;

	bool m_initialized = false;

	Swapchain& m_swapchain;
	ResourceManager& m_resourceManager;
	MaterialManager& m_materialManager;
	const RenderingConfiguration& m_renderingConfiguration;
	TaskRegister& m_taskRegister;

	std::optional<ResourceManager::AllocationIndex> m_deviceAllocation;
	std::optional<ResourceManager::AllocationIndex> m_sharedAllocation;
	std::optional<ResourceManager::AllocationIndex> m_hostAllocation;

	IndexMap<ResourceIndex, ImageHandle> m_images;
	IndexMap<ResourceIndex, BufferHandle> m_buffers;

	std::array<vk::Semaphore, 3> m_renderSemaphores = {};

	vk::QueryPool m_debugQueryPool;
	std::chrono::time_point<std::chrono::steady_clock> m_beginFrameTimestamp;
	double m_lastCpuFrameTime;

	uint64_t m_currentFrame = 0;

	bool updateTimings() const;
	void outputToSwapchain(vk::CommandBuffer& commandBuffer, uint32_t imageIndex) const;

public:
	RenderGraphRunner(
		Swapchain& swapchain,
		ResourceManager& resourceManager,
		MaterialManager& materialManager,
		const RenderingConfiguration& renderingConfiguration,
		ExecutionInfo execInfo,
		TaskRegister& taskRegister
	);
	~RenderGraphRunner();

	bool submit(const Scene& scene);
};
}  // namespace rendergraph::internal
