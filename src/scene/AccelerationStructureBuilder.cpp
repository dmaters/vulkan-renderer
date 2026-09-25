#include "AccelerationStructureBuilder.hpp"

#include "Instance.hpp"

struct BLASInfo {
	std::vector<vk::AccelerationStructureGeometryKHR> geometries;
	std::vector<vk::AccelerationStructureBuildGeometryInfoKHR> geometryInfo;
};
BLASInfo buildBLASInfos(const std::vector<Primitive>& primitives, Buffer& vertexBuffer, Buffer& indexBuffer) {
	vk::DeviceAddress vertexAddress =
		Instance::Get().device.getBufferAddress(vk::BufferDeviceAddressInfo { .buffer = vertexBuffer.buffer });
	vk::DeviceAddress indexAddress =
		Instance::Get().device.getBufferAddress(vk::BufferDeviceAddressInfo { .buffer = indexBuffer.buffer });

	std::vector<vk::AccelerationStructureGeometryKHR> geometries(primitives.size());
	std::vector<vk::AccelerationStructureBuildGeometryInfoKHR> geometryInfo(primitives.size());

	for (int i = 0; i < primitives.size(); i++) {
		auto& primitive = primitives[i];

		vk::AccelerationStructureGeometryTrianglesDataKHR triangleData {
			.vertexFormat = vk::Format::eR32G32B32Sfloat,
			.vertexData = { vertexAddress + primitive.baseVertex * sizeof(glm::vec3) },
			.vertexStride = sizeof(glm::vec3),
			.maxVertex = primitive.vertexCount - 1,
			.indexType = vk::IndexType::eUint32,
			.indexData = { indexAddress + primitive.baseIndex * sizeof(uint32_t) },
		};

		geometries[i] = vk::AccelerationStructureGeometryKHR {
			.geometryType = vk::GeometryTypeKHR::eTriangles,
			.geometry = { .triangles = triangleData },
			.flags = vk::GeometryFlagBitsKHR::eOpaque,
		};

		geometryInfo[i] = vk::AccelerationStructureBuildGeometryInfoKHR {
			.type = vk::AccelerationStructureTypeKHR::eBottomLevel,
			.mode = vk::BuildAccelerationStructureModeKHR::eBuild,
			.geometryCount = 1,
			.pGeometries = &(geometries[i]),
		};
	}

	return {
		.geometries = std::move(geometries),
		.geometryInfo = std::move(geometryInfo),
	};
}

struct Allocations {
	ResourceManager::AllocationIndex AS;
	ResourceManager::AllocationIndex scratch;
};

Allocations allocateASMemory(
	const vk::AccelerationStructureBuildSizesInfoKHR& sizes, ResourceManager& resourceManager
) {
	ResourceManager::BufferDescription BLASBuffer {
		.size = (uint32_t)sizes.accelerationStructureSize,
		.usage =
			vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
	};

	auto blasAllocation = resourceManager.createResources({}, { BLASBuffer }, ResourceManager::MemoryLocation::Device);

	ResourceManager::BufferDescription scratchBuffer {
		.size = (uint32_t)sizes.buildScratchSize,
		.usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
				 vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer,
	};

	auto scratchAllocation =
		resourceManager.createResources({}, { scratchBuffer }, ResourceManager::MemoryLocation::Device);

	return { blasAllocation, scratchAllocation };
}

void preLoadBarrier(vk::CommandBuffer& commandBuffer, Buffer& vertexBuffer, Buffer& indexBuffer) {
	std::array<vk::BufferMemoryBarrier2, 2> barriers;
	barriers[0] = {
		.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
		.dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureReadKHR,

		.buffer = vertexBuffer.buffer,
		.size = vertexBuffer.size,
	};
	barriers[1] = {
		.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
		.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
		.dstStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
		.dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureReadKHR,

		.buffer = indexBuffer.buffer,
		.size = indexBuffer.size,
	};
	commandBuffer.pipelineBarrier2(
		vk::DependencyInfo {
			.bufferMemoryBarrierCount = (uint32_t)barriers.size(),
			.pBufferMemoryBarriers = barriers.data(),
		}
	);
}
void preLoadBarrier(
	vk::CommandBuffer& commandBuffer, ResourceManager::AllocationIndex blasAllocation, ResourceManager& resourceManager
) {
	std::vector<vk::BufferMemoryBarrier2> barriers;
	auto& handles = resourceManager.getBuffers(blasAllocation);
	barriers.reserve(handles.size());

	for (auto& handle : handles) {
		auto& buffer = resourceManager.getBuffer(handle);

		barriers.push_back(
			vk::BufferMemoryBarrier2 {
				.srcStageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
				.srcAccessMask = vk::AccessFlagBits2::eAccelerationStructureWriteKHR,
				.dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
				.dstAccessMask = vk::AccessFlagBits2::eAccelerationStructureReadKHR,

				.buffer = buffer.buffer,
				.size = buffer.size,
			}
		);
	}

	commandBuffer.pipelineBarrier2(
		vk::DependencyInfo {
			.bufferMemoryBarrierCount = (uint32_t)barriers.size(),
			.pBufferMemoryBarriers = barriers.data(),
		}
	);
}

AccelerationStructureBuilder::BLASData AccelerationStructureBuilder::buildBLAS(
	vk::CommandBuffer& commandBuffer, const AccelerationStructureBuilder::BuildBLASInfo& info
) {
	auto& device = Instance::Get().device;

	preLoadBarrier(commandBuffer, info.vertexBuffer, info.indexBuffer);
	auto BLASInfos = buildBLASInfos(info.primitives, info.vertexBuffer, info.indexBuffer);

	std::vector<vk::AccelerationStructureBuildSizesInfoKHR> sizes(info.primitives.size());
	std::vector<ResourceManager::BufferDescription> blasDescriptions(info.primitives.size());
	std::vector<ResourceManager::BufferDescription> scratchBuffersDescriptions(info.primitives.size());

	for (int i = 0; i < info.primitives.size(); i++) {
		sizes[i] = device.getAccelerationStructureBuildSizesKHR(
			vk::AccelerationStructureBuildTypeKHR::eDevice, BLASInfos.geometryInfo[i], info.primitives[i].indexCount / 3
		);

		blasDescriptions[i] = {
			.size = (uint32_t)sizes[i].accelerationStructureSize,
			.usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
					 vk::BufferUsageFlagBits::eShaderDeviceAddress,
		};
		scratchBuffersDescriptions[i] = {
			.size = (uint32_t)sizes[i].buildScratchSize,
			.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
		};
	}
	auto sceneBLASAllocation =
		m_resourceManager.createResources({}, blasDescriptions, ResourceManager::MemoryLocation::Device);
	auto sceneScratchAllocation =
		m_resourceManager.createResources({}, scratchBuffersDescriptions, ResourceManager::MemoryLocation::Device);

	auto blasBuffersHandles = m_resourceManager.getBuffers(sceneBLASAllocation);

	std::vector<vk::AccelerationStructureKHR> blas(info.primitives.size());

	std::vector<vk::AccelerationStructureBuildRangeInfoKHR> buildRangesData(info.primitives.size());
	std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> buildRanges(info.primitives.size());

	for (int i = 0; i < info.primitives.size(); i++) {
		auto& buffer = m_resourceManager.getBuffer(blasBuffersHandles[i]);

		vk::AccelerationStructureCreateInfoKHR createInfo = {
			.buffer = buffer.buffer,
			.offset = 0,
			.size = buffer.size,
			.type = vk::AccelerationStructureTypeKHR::eBottomLevel,
		};

		blas[i] = device.createAccelerationStructureKHR(createInfo);

		BLASInfos.geometryInfo[i].dstAccelerationStructure = blas[i];

		auto scratchBufferAddress = device.getBufferAddress(
			{ .buffer = m_resourceManager.getBuffer(m_resourceManager.getBuffers(sceneScratchAllocation)[i]).buffer }
		);

		BLASInfos.geometryInfo[i].scratchData = vk::DeviceOrHostAddressKHR(scratchBufferAddress);

		buildRangesData[i] = vk::AccelerationStructureBuildRangeInfoKHR {
			.primitiveCount = info.primitives[i].indexCount / 3,
			.primitiveOffset = 0,
			.firstVertex = 0,
			.transformOffset = 0,
		};
		buildRanges[i] = &(buildRangesData[i]);
	}

	commandBuffer.buildAccelerationStructuresKHR(
		BLASInfos.geometryInfo.size(), BLASInfos.geometryInfo.data(), buildRanges.data()
	);
	preLoadBarrier(commandBuffer, sceneBLASAllocation, m_resourceManager);
	return {
		.allocation = sceneBLASAllocation,
		.blas = std::move(blas),
	};
}

vk::TransformMatrixKHR getTransform(const glm::mat4& transform) {
	vk::TransformMatrixKHR res;
	res.matrix[0][0] = transform[0][0];
	res.matrix[0][1] = transform[1][0];
	res.matrix[0][2] = transform[2][0];
	res.matrix[0][3] = transform[3][0];

	res.matrix[1][0] = transform[0][1];
	res.matrix[1][1] = transform[1][1];
	res.matrix[1][2] = transform[2][1];
	res.matrix[1][3] = transform[3][1];

	res.matrix[2][0] = transform[0][2];
	res.matrix[2][1] = transform[1][2];
	res.matrix[2][2] = transform[2][2];
	res.matrix[2][3] = transform[3][2];

	return res;
}

AccelerationStructureBuilder::TLASData AccelerationStructureBuilder::buildTLAS(
	vk::CommandBuffer& commandBuffer, const AccelerationStructureBuilder::BuildTLASInfo& info
) {
	auto& device = Instance::Get().device;

	ResourceManager::BufferDescription instanceBufferInfo = {
		.size = static_cast<uint32_t>(info.primitives.size() * sizeof(vk::AccelerationStructureInstanceKHR)),
		.usage = vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
				 vk::BufferUsageFlagBits::eShaderDeviceAddress
	};
	auto instanceAllocation =
		m_resourceManager.createResources({}, { instanceBufferInfo }, ResourceManager::MemoryLocation::HostVisible);

	auto instanceBuffer = m_resourceManager.getBuffer(m_resourceManager.getBuffers(instanceAllocation)[0]);
	auto instances = (vk::AccelerationStructureInstanceKHR*)instanceBuffer.data;

	for (int i = 0; i < info.primitives.size(); i++) {
		vk::AccelerationStructureDeviceAddressInfoKHR addrInfo { .accelerationStructure = info.blas[i] };
		vk::DeviceAddress blasDeviceAddr = device.getAccelerationStructureAddressKHR(addrInfo);

		instances[i] = vk::AccelerationStructureInstanceKHR {
			.transform = getTransform(info.transforms[i]),
			.mask = 0xFF,
			.accelerationStructureReference = blasDeviceAddr,
		};
	}

	auto instanceBufferDeviceAddress =
		device.getBufferAddress(vk::BufferDeviceAddressInfo { .buffer = instanceBuffer.buffer });

	vk::AccelerationStructureGeometryInstancesDataKHR instanceData {
		.arrayOfPointers = vk::False,
		.data = {
		    .deviceAddress = instanceBufferDeviceAddress,
	    },
	};
	vk::AccelerationStructureGeometryDataKHR geometryData { .instances = instanceData };

	vk::AccelerationStructureGeometryKHR tlasGeometry {
		.geometryType = vk::GeometryTypeKHR::eInstances,
		.geometry = geometryData,

	};

	vk::AccelerationStructureBuildGeometryInfoKHR geometryInfo {
		.type = vk::AccelerationStructureTypeKHR::eTopLevel,
		.mode = vk::BuildAccelerationStructureModeKHR::eBuild,
		.geometryCount = 1,
		.pGeometries = &tlasGeometry,

	};

	auto sizes = device.getAccelerationStructureBuildSizesKHR(
		vk::AccelerationStructureBuildTypeKHR::eDevice, geometryInfo, info.primitives.size()
	);

	auto allocations = allocateASMemory(sizes, m_resourceManager);

	vk::AccelerationStructureCreateInfoKHR createInfo {
		.buffer = m_resourceManager.getBuffer(m_resourceManager.getBuffers(allocations.AS)[0]).buffer,
		.offset = 0,
		.size = sizes.accelerationStructureSize,
		.type = vk::AccelerationStructureTypeKHR::eTopLevel,
	};

	auto tlas = device.createAccelerationStructureKHR(createInfo);

	geometryInfo.dstAccelerationStructure = tlas;

	vk::AccelerationStructureBuildRangeInfoKHR rangeInfo {
		.primitiveCount = (uint32_t)info.primitives.size(),
		.primitiveOffset = 0,
		.firstVertex = 0,
		.transformOffset = 0,
	};

	geometryInfo.scratchData.deviceAddress = device.getBufferAddress(
		vk::BufferDeviceAddressInfo {
			.buffer = m_resourceManager.getBuffer(m_resourceManager.getBuffers(allocations.scratch)[0]).buffer,

		}
	);

	commandBuffer.buildAccelerationStructuresKHR({ geometryInfo }, { &rangeInfo });

	return {
		.allocation = allocations.AS,
		.tlas = tlas,
	};
}
