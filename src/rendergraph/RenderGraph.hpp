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
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"

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

	Swapchain& m_swapchain;
	ResourceManager& m_resourceManager;
	MaterialManager& m_materialManager;

	std::unordered_map<std::string_view, ResourceManager::BufferDescription>
		m_bufferCreationsInfos;
	std::unordered_map<std::string_view, ResourceManager::ImageDescription>
		m_imageCreationInfos;
	std::unordered_map<std::string_view, uint8_t> m_swapchainDependentImages;

	std::array<std::vector<ImageHandle>, 3> m_unusedImages;
	std::array<std::vector<BufferHandle>, 3> m_unusedBuffers;
	uint8_t m_swapchainFlushCounter = 0;

	GraphData m_data;
	ImageHandle m_resultHandle = { 0 };

	uint8_t m_currentFrame = 0;

	void writeInitialSyncronizationBarrier(vk::CommandBuffer& buffer);

	void writeMemoryBarrier(
		vk::CommandBuffer& commandBuffer, GraphData::TaskData& task
	) const;

	void initializeExternalImages(vk::CommandBuffer& commandBuffer);

	void outputToSwapchain(vk::CommandBuffer& commandBuffer, uint32_t index);
	void clearUnusedResources();

	void rebuildSwapchain();

public:
	RenderGraph(
		Swapchain& swapchain,
		ResourceManager& resourceManager,
		MaterialManager& materialManager
	);

	void addImage(
		std::string_view name,
		ResourceManager::ImageDescription description,
		uint8_t swapchainResolutionMultiplier
	);

	void addBuffer(
		std::string_view name, ResourceManager::BufferDescription description
	);

	void submit(const Scene& scene);
	void build(GraphData buildData);
};
