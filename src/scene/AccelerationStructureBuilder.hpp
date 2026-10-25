#pragma once

#include "Primitive.hpp"
#include "resources/ResourceManager.hpp"

class AccelerationStructureBuilder {
public:
	struct BLASAllocationInfo;
	BLASAllocationInfo queryBLASAllocationInfo(std::span<Primitive> primitives);

	struct TLASAllocationInfo;
	TLASAllocationInfo queryTLASAllocationInfo(std::size_t primitiveInstancesCount);

	struct BuildBLASInfo;
	struct BLASData;
	std::vector<vk::AccelerationStructureKHR> buildBLAS(vk::CommandBuffer& commandBuffer, const BuildBLASInfo& info);

	struct BuildTLASInfo;
	struct TLASData;
	vk::AccelerationStructureKHR buildTLAS(vk::CommandBuffer& commandBuffer, const BuildTLASInfo& info);
};

struct AccelerationStructureBuilder::BLASAllocationInfo {
	std::vector<ResourceManager::BufferDescription> asBuffers;
	std::vector<ResourceManager::BufferDescription> scratchBuffers;
};

struct AccelerationStructureBuilder::TLASAllocationInfo {
	ResourceManager::BufferDescription instanceData;
	ResourceManager::BufferDescription asBuffer;
	ResourceManager::BufferDescription scratchBuffer;
};

struct AccelerationStructureBuilder::BuildBLASInfo {
	std::span<Primitive> primitives;
	std::span<const BufferHandle> blasBuffers;
	std::span<const BufferHandle> scratchBuffers;
	BufferHandle vertexBuffer;
	BufferHandle indexBuffer;
	ResourceManager& resourceManager;
};
struct AccelerationStructureBuilder::BLASData {
	std::vector<vk::AccelerationStructureKHR> blas;
};

struct AccelerationStructureBuilder::BuildTLASInfo {
	std::span<Primitive> primitives;
	std::span<vk::AccelerationStructureKHR> blas;
	std::span<glm::mat4> transforms;

	BufferHandle instancesBuffer;
	BufferHandle asBuffer;
	BufferHandle scratchBuffer;

	ResourceManager& resourceManager;
};

struct AccelerationStructureBuilder::TLASData {
	ResourceManager::AllocationIndex allocation;
	vk::AccelerationStructureKHR tlas;
};
