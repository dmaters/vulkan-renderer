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
	BLASData buildBLAS(vk::CommandBuffer& commandBuffer, const BuildBLASInfo& info);

	struct BuildTLASInfo;
	struct TLASData;
	TLASData buildTLAS(vk::CommandBuffer& commandBuffer, const BuildTLASInfo& info);
};

struct AccelerationStructureBuilder::BuildBLASInfo {
	std::vector<Primitive>& primitives;
	vk::Buffer& vertexBuffer;
	vk::Buffer& indexBuffer;
};
struct AccelerationStructureBuilder::BLASData {
	ResourceManager::AllocationIndex allocation;
	std::vector<vk::AccelerationStructureKHR> blas;
};

struct AccelerationStructureBuilder::BuildTLASInfo {
	std::vector<Primitive>& primitives;
	std::vector<vk::AccelerationStructureKHR>& blas;
	std::vector<glm::mat4>& transforms;
};

struct AccelerationStructureBuilder::TLASData {
	ResourceManager::AllocationIndex allocation;
	vk::AccelerationStructureKHR tlas;
};
