#pragma once

#include <vector>
#include <vulkan/vulkan.hpp>

#include "SceneLoader.hpp"
#include "resources/ResourceWriteTransaction.hpp"

class AccelerationStructureManager {
public:
	struct TLASData {
		ResourceManager::AllocationIndex allocationIndex;
		vk::AccelerationStructureKHR accelerationStructure;
	};

private:
	std::vector<vk::AccelerationStructureKHR> m_blases;

	std::vector<ResourceManager::AllocationIndex> m_blasAllocations;
	ResourceManager& m_resourceManager;

	std::optional<ResourceManager::AllocationIndex> m_blasScratcAllocation;
	std::optional<ResourceManager::AllocationIndex> m_tlasScratchAllocation;
	std::optional<ResourceManager::AllocationIndex> m_instanceDataAllocation;

	std::optional<TLASData> m_tlasData;

public:
	AccelerationStructureManager(ResourceManager& resourceManager) : m_resourceManager(resourceManager) {}

	void addScene(
		SceneLoader::SceneInstance& scene,
		BufferHandle vertexBuffer,
		BufferHandle indexBuffer,
		ResourceWriteTransaction& transaction
	);

	TLASData buildTLAS(Scene& scene, ResourceWriteTransaction& transaction);

	void cleanup();
};
