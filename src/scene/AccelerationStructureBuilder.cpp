#include "AccelerationStructureBuilder.hpp"

#include "Instance.hpp"

AccelerationStructureBuilder::BLASAllocationInfo AccelerationStructureBuilder::queryBLASAllocationInfo(
	std::span<Primitive> primitives
) {
	auto& device = Instance::Get().device;

	std::vector<ResourceManager::BufferDescription> asBuffers;
	asBuffers.reserve(primitives.size());
	std::vector<ResourceManager::BufferDescription> scratchBuffers;
	scratchBuffers.reserve(primitives.size());

	for (auto& primitive : primitives) {
		vk::AccelerationStructureGeometryTrianglesDataKHR triangleLayout {
			.vertexFormat = vk::Format::eR32G32B32Sfloat,
			.indexType = vk::IndexType::eUint32,
		};

		vk::AccelerationStructureGeometryKHR geometry {
			.geometryType = vk::GeometryTypeKHR::eTriangles,
			.geometry = { .triangles = triangleLayout },
			.flags = vk::GeometryFlagBitsKHR::eOpaque,
		};
		vk::AccelerationStructureBuildGeometryInfoKHR geometryInfo {
			.type = vk::AccelerationStructureTypeKHR::eBottomLevel,
			.mode = vk::BuildAccelerationStructureModeKHR::eBuild,
			.geometryCount = 1,
			.pGeometries = &geometry,
		};

		auto sizes = device.getAccelerationStructureBuildSizesKHR(
			vk::AccelerationStructureBuildTypeKHR::eDevice, geometryInfo, primitive.indexCount / 3
		);

		asBuffers.push_back(
			{
				.size = (uint32_t)sizes.accelerationStructureSize,
				.usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR,
			}
		);

		scratchBuffers.push_back(
			{
				.size = (uint32_t)sizes.buildScratchSize,
				.usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddressEXT,
			}
		);
	}

	return BLASAllocationInfo {
		.asBuffers = std::move(asBuffers),
		.scratchBuffers = std::move(scratchBuffers),
	};
}

AccelerationStructureBuilder::TLASAllocationInfo AccelerationStructureBuilder::queryTLASAllocationInfo(
	std::size_t primitiveInstancesCount
) {
	auto& device = Instance::Get().device;
	vk::AccelerationStructureGeometryKHR geometry {
		.geometryType = vk::GeometryTypeKHR::eInstances,
	};
	vk::AccelerationStructureBuildGeometryInfoKHR tlasGeometryInfo {
		.type = vk::AccelerationStructureTypeKHR::eTopLevel,
		.mode = vk::BuildAccelerationStructureModeKHR::eBuild,
		.geometryCount = 1,
		.pGeometries = &geometry,
	};

	auto tlasSizes = device.getAccelerationStructureBuildSizesKHR(
		vk::AccelerationStructureBuildTypeKHR::eDevice, tlasGeometryInfo, primitiveInstancesCount
	);

	ResourceManager::BufferDescription instanceBufferInfo = {
		.size = static_cast<uint32_t>(primitiveInstancesCount * sizeof(vk::AccelerationStructureInstanceKHR)),
		.usage = vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
				 vk::BufferUsageFlagBits::eShaderDeviceAddress
	};

	ResourceManager::BufferDescription asDescripton {
		.size = (uint32_t)tlasSizes.accelerationStructureSize,
		.usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR,
	};

	ResourceManager::BufferDescription scratchDescription {
		.size = (uint32_t)tlasSizes.buildScratchSize,
		.usage = vk::BufferUsageFlagBits::eStorageBuffer,
	};

	return {
		.instanceData = instanceBufferInfo,
		.asBuffer = asDescripton,
		.scratchBuffer = scratchDescription,
	};
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
	vk::CommandBuffer& commandBuffer, std::span<const BufferHandle> blasHandles, ResourceManager& resourceManager
) {
	std::vector<vk::BufferMemoryBarrier2> barriers;

	for (auto& handle : blasHandles) {
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

std::vector<vk::AccelerationStructureKHR> AccelerationStructureBuilder::buildBLAS(
	vk::CommandBuffer& commandBuffer, const AccelerationStructureBuilder::BuildBLASInfo& info
) {
	auto& device = Instance::Get().device;
	auto& vertexBuffer = info.resourceManager.getBuffer(info.vertexBuffer);
	auto& indexBuffer = info.resourceManager.getBuffer(info.indexBuffer);
	preLoadBarrier(commandBuffer, vertexBuffer, indexBuffer);

	vk::DeviceAddress vertexAddress =
		Instance::Get().device.getBufferAddress(vk::BufferDeviceAddressInfo { .buffer = vertexBuffer.buffer });
	vk::DeviceAddress indexAddress =
		Instance::Get().device.getBufferAddress(vk::BufferDeviceAddressInfo { .buffer = indexBuffer.buffer });

	std::vector<vk::AccelerationStructureKHR> blas(info.primitives.size());

	std::vector<vk::AccelerationStructureGeometryKHR> geometries(info.primitives.size());
	std::vector<vk::AccelerationStructureBuildGeometryInfoKHR> geometryInfos(info.primitives.size());
	std::vector<vk::AccelerationStructureBuildRangeInfoKHR> buildRangesData(info.primitives.size());
	std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> buildRanges(info.primitives.size());

	for (int i = 0; i < info.primitives.size(); i++) {
		auto& primitive = info.primitives[i];
		auto& buffer = info.resourceManager.getBuffer(info.blasBuffers[i]);

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

		geometryInfos[i] = vk::AccelerationStructureBuildGeometryInfoKHR {
			.type = vk::AccelerationStructureTypeKHR::eBottomLevel,
			.mode = vk::BuildAccelerationStructureModeKHR::eBuild,
			.geometryCount = 1,
			.pGeometries = &(geometries[i]),
		};

		vk::AccelerationStructureCreateInfoKHR createInfo = {
			.buffer = buffer.buffer,
			.offset = 0,
			.size = buffer.size,
			.type = vk::AccelerationStructureTypeKHR::eBottomLevel,
		};

		blas[i] = device.createAccelerationStructureKHR(createInfo);

		geometryInfos[i].dstAccelerationStructure = blas[i];

		auto scratchBufferAddress =
			device.getBufferAddress({ .buffer = info.resourceManager.getBuffer(info.scratchBuffers[i]).buffer });

		geometryInfos[i].scratchData = vk::DeviceOrHostAddressKHR(scratchBufferAddress);

		buildRangesData[i] = vk::AccelerationStructureBuildRangeInfoKHR {
			.primitiveCount = info.primitives[i].indexCount / 3,
			.primitiveOffset = 0,
			.firstVertex = 0,
			.transformOffset = 0,
		};

		buildRanges[i] = &(buildRangesData[i]);
	}

	commandBuffer.buildAccelerationStructuresKHR(geometryInfos.size(), geometryInfos.data(), buildRanges.data());

	preLoadBarrier(commandBuffer, info.blasBuffers, info.resourceManager);
	return blas;
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

vk::AccelerationStructureKHR AccelerationStructureBuilder::buildTLAS(
	vk::CommandBuffer& commandBuffer, const AccelerationStructureBuilder::BuildTLASInfo& info
) {
	auto& device = Instance::Get().device;

	auto instanceBuffer = info.resourceManager.getBuffer(info.instancesBuffer);
	auto instances = (vk::AccelerationStructureInstanceKHR*)instanceBuffer.data;

	for (int i = 0; i < info.primitives.size(); i++) {
		vk::AccelerationStructureDeviceAddressInfoKHR addrInfo { .accelerationStructure = info.blas[i] };
		vk::DeviceAddress blasDeviceAddr = device.getAccelerationStructureAddressKHR(addrInfo);

		instances[i] = vk::AccelerationStructureInstanceKHR {
			.transform = getTransform(info.transforms[i]),
			.instanceCustomIndex = (uint32_t)i,
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
	auto scratchBufferAddress = device.getBufferAddress(
		vk::BufferDeviceAddressInfo {
			.buffer = info.resourceManager.getBuffer(info.scratchBuffer).buffer,

		}
	);
	vk::AccelerationStructureBuildGeometryInfoKHR geometryInfo {
		.type = vk::AccelerationStructureTypeKHR::eTopLevel,
		.mode = vk::BuildAccelerationStructureModeKHR::eBuild,
		.geometryCount = 1,
		.pGeometries = &tlasGeometry,
		.scratchData = { .deviceAddress = scratchBufferAddress },
	};

	auto& asBuffer = info.resourceManager.getBuffer(info.asBuffer);

	vk::AccelerationStructureCreateInfoKHR createInfo {
		.buffer = asBuffer.buffer,
		.offset = 0,
		.size = asBuffer.size,
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

	commandBuffer.buildAccelerationStructuresKHR({ geometryInfo }, { &rangeInfo });

	return tlas;
}
