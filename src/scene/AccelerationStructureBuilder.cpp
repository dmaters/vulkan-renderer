#include "AccelerationStructureBuilder.hpp"

#include "Instance.hpp"

struct BLASInfo {
	std::vector<vk::AccelerationStructureGeometryTrianglesDataKHR> geometryData;
	std::vector<vk::AccelerationStructureGeometryKHR> geometries;
	std::vector<vk::AccelerationStructureBuildGeometryInfoKHR> geometryInfo;
};
BLASInfo buildBLASInfos(const std::vector<Primitive>& primitives, vk::Buffer vertexBuffer, vk::Buffer indexBuffer) {
	vk::DeviceAddress vertexAddress =
		Instance::Get().device.getBufferAddress(vk::BufferDeviceAddressInfo { .buffer = vertexBuffer });
	vk::DeviceAddress indexAddress =
		Instance::Get().device.getBufferAddress(vk::BufferDeviceAddressInfo { .buffer = indexBuffer });

	std::vector<vk::AccelerationStructureGeometryTrianglesDataKHR> geometryData(primitives.size());
	std::vector<vk::AccelerationStructureGeometryKHR> geometries(primitives.size());
	std::vector<vk::AccelerationStructureBuildGeometryInfoKHR> geometryInfo(primitives.size());

	for (int i = 0; i < primitives.size(); i++) {
		auto& primitive = primitives[i];

		geometryData[i] = vk::AccelerationStructureGeometryTrianglesDataKHR {
			.vertexFormat = vk::Format::eR32G32B32Sfloat,
			.vertexData = { vertexAddress + primitive.baseVertex * sizeof(uint32_t) },
			.vertexStride = sizeof(glm::vec3),
			.maxVertex = primitive.vertexCount,
			.indexType = vk::IndexType::eUint32,
			.indexData = { indexAddress + primitive.baseIndex * sizeof(uint32_t) },
		};

		geometries[i] = vk::AccelerationStructureGeometryKHR {
			.geometryType = vk::GeometryTypeKHR::eTriangles,
			.geometry = { geometryData[i] },
			.flags = vk::GeometryFlagBitsKHR::eOpaque,
		};

		geometryInfo[i] = vk::AccelerationStructureBuildGeometryInfoKHR {
			.type = vk::AccelerationStructureTypeKHR::eBottomLevel,
			.mode = vk::BuildAccelerationStructureModeKHR::eBuild,
			.geometryCount = 1,
			.pGeometries = &geometries[i],
		};
	}

	return {
		.geometryData = geometryData,
		.geometries = geometries,
		.geometryInfo = geometryInfo,
	};
}

struct Allocations {
	ResourceManager::AllocationIndex BLAS;
	ResourceManager::AllocationIndex scratch;
};

Allocations allocateBLASMemory(
	const vk::AccelerationStructureBuildSizesInfoKHR& sizes, ResourceManager& resourceManager
) {
	ResourceManager::BufferDescription BLASBuffer {
		.size = (uint32_t)sizes.accelerationStructureSize,
		.usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR,
	};

	auto blasAllocation = resourceManager.createResources({}, { BLASBuffer }, ResourceManager::MemoryLocation::Device);

	ResourceManager::BufferDescription scratchBuffer {
		.size = (uint32_t)sizes.buildScratchSize,
		.usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR,
	};

	auto scratchAllocation =
		resourceManager.createResources({}, { scratchBuffer }, ResourceManager::MemoryLocation::Device);

	return { blasAllocation, scratchAllocation };
}

AccelerationStructureBuilder::BLASData AccelerationStructureBuilder::buildBLAS(
	vk::CommandBuffer& commandBuffer, AccelerationStructureBuilder::BuildBLASInfo& info
) {
	auto& device = Instance::Get().device;
	auto BLASInfos = buildBLASInfos(info.primitives, info.vertexBuffer, info.indexBuffer);

	std::vector<vk::AccelerationStructureBuildSizesInfoKHR> sizes(info.primitives.size());
	std::vector<ResourceManager::BufferDescription> descriptions(info.primitives.size());
	ResourceManager::BufferDescription scratchBufferDescription {
		.size = 0,
		.usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR,
	};
	for (int i = 0; i < info.primitives.size(); i++) {
		const uint32_t maxPrimitiveCount = 1;
		sizes[i] = device.getAccelerationStructureBuildSizesKHR(
			vk::AccelerationStructureBuildTypeKHR::eDevice, BLASInfos.geometryInfo[i], maxPrimitiveCount
		);

		descriptions[i] = {
			.size = (uint32_t)sizes[i].buildScratchSize,
			.usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR,
		};
		scratchBufferDescription.size += sizes[i].buildScratchSize;
	}
	auto sceneBLASAllocation =
		m_resourceManager.createResources({}, descriptions, ResourceManager::MemoryLocation::Device);
	auto sceneScratchAllocation =
		m_resourceManager.createResources({}, { scratchBufferDescription }, ResourceManager::MemoryLocation::Device);

	auto blasBuffersHandles = m_resourceManager.getBuffers(sceneBLASAllocation);

	std::vector<vk::AccelerationStructureKHR> blas(info.primitives.size());

	auto scratchBufferAddress = device.getBufferAddress(
		{ .buffer = m_resourceManager.getBuffer(m_resourceManager.getBuffers(sceneScratchAllocation)[0]).buffer }
	);

	std::size_t scratchBufferOffset = 0;

	std::vector<vk::AccelerationStructureBuildRangeInfoKHR> buildRangesData(info.primitives.size());
	std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> buildRanges(info.primitives.size());

	for (int i = 0; i < info.primitives.size(); i++) {
		auto& buffer = m_resourceManager.getBuffer(blasBuffersHandles[i]);

		vk::AccelerationStructureCreateInfoKHR info = {
			.buffer = buffer.buffer,
			.offset = 0,
			.size = buffer.size,
			.type = vk::AccelerationStructureTypeKHR::eBottomLevel,
		};

		blas[i] = device.createAccelerationStructureKHR(info);

		BLASInfos.geometryInfo[i].dstAccelerationStructure = blas[i];
		BLASInfos.geometryInfo[i].scratchData = vk::DeviceOrHostAddressKHR(scratchBufferAddress + scratchBufferOffset);

		buildRangesData[i] = {
			.primitiveCount = 1,
			.primitiveOffset = 0,
			.firstVertex = 0,
			.transformOffset = 0,
		};
		buildRanges[i] = &buildRangesData[i];
	}

	commandBuffer.buildAccelerationStructuresKHR(
		BLASInfos.geometryInfo.size(), BLASInfos.geometryInfo.data(), buildRanges.data()
	);

	return {
		.allocation = sceneBLASAllocation,
		.blas = blas,
	};
}
