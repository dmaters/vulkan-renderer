#pragma once

#include "Primitive.hpp"
#include "resources/ResourceManager.hpp"

class AccelerationStructureBuilder {
private:
	ResourceManager& m_resourceManager;

public:
	AccelerationStructureBuilder(ResourceManager& resourceManager) : m_resourceManager(resourceManager) {}

	struct BuildBLASInfo;
	struct BLASData;
	BLASData buildBLAS(vk::CommandBuffer& commandBuffer, BuildBLASInfo& info);

	struct UpdateTLASInfo;
	ResourceManager::AllocationIndex updateTLAS(UpdateTLASInfo& info);
};

struct AccelerationStructureBuilder::BuildBLASInfo {
	std::vector<Primitive> primitives;
	vk::Buffer& vertexBuffer;
	vk::Buffer& indexBuffer;
};
struct AccelerationStructureBuilder::BLASData {
	ResourceManager::AllocationIndex allocation;
	std::vector<vk::AccelerationStructureKHR> blas;
};
