#include "AccelerationStructureManager.hpp"

#include "AccelerationStructureBuilder.hpp"

void AccelerationStructureManager::addScene(
	SceneLoader::SceneInstance& scene,
	BufferHandle vertexBuffer,
	BufferHandle indexBuffer,
	ResourceWriteTransaction& transaction
) {
	AccelerationStructureBuilder builder;

	AccelerationStructureBuilder asBuilder;
	auto blasAllocationInfo = asBuilder.queryBLASAllocationInfo(scene.primitives);
	std::vector<ResourceManager::BufferDescription> scratchDescriptions(blasAllocationInfo.scratchBuffers);

	auto blasAllocation =
		m_resourceManager.createResources({}, blasAllocationInfo.asBuffers, ResourceManager::MemoryLocation::Device);

	auto scratchAllocation = m_resourceManager.createResources(
		{}, blasAllocationInfo.scratchBuffers, ResourceManager::MemoryLocation::Device
	);

	transaction.customOperation([&](vk::CommandBuffer& commandBuffer) {
		auto blases = builder.buildBLAS(
			commandBuffer,
			{
				.primitives = scene.primitives,
				.blasBuffers = m_resourceManager.getBuffers(blasAllocation),
				.scratchBuffers = m_resourceManager.getBuffers(scratchAllocation),
				.vertexBuffer = vertexBuffer,
				.indexBuffer = indexBuffer,
				.resourceManager = m_resourceManager,
			}
		);

		m_blases.insert(m_blases.end(), blases.begin(), blases.end());
	});

	m_blasAllocations.push_back(blasAllocation);
	m_blasScratcAllocation = scratchAllocation;
}

AccelerationStructureManager::TLASData AccelerationStructureManager::buildTLAS(
	Scene& scene, ResourceWriteTransaction& transaction
) {
	vk::AccelerationStructureKHR tlas;
	AccelerationStructureBuilder builder;

	auto tlasAllocationInfo = builder.queryTLASAllocationInfo(scene.primitives.size());

	auto tlasAllocation =
		m_resourceManager.createResources({}, { tlasAllocationInfo.asBuffer }, ResourceManager::MemoryLocation::Device);

	auto instanceDataAllocation = m_resourceManager.createResources(
		{}, { tlasAllocationInfo.instanceData }, ResourceManager::MemoryLocation::HostVisible
	);

	auto scratchAllocation = m_resourceManager.createResources(
		{}, { tlasAllocationInfo.scratchBuffer }, ResourceManager::MemoryLocation::Device
	);

	transaction.customOperation([&](vk::CommandBuffer& commandBuffer) {
		tlas = builder.buildTLAS(
			commandBuffer,
			{
				.primitives = scene.primitives,
				.blas = m_blases,
				.transforms = scene.transforms,
				.instancesBuffer = m_resourceManager.getBuffers(instanceDataAllocation)[0],
				.asBuffer = m_resourceManager.getBuffers(tlasAllocation)[9],
				.scratchBuffer = m_resourceManager.getBuffers(scratchAllocation)[0],
				.resourceManager = m_resourceManager,
			}
		);
	});

	m_instanceDataAllocation = instanceDataAllocation;
	m_tlasScratchAllocation = scratchAllocation;

	m_tlasData = TLASData {
		.allocationIndex = tlasAllocation,
		.accelerationStructure = tlas,
	};
	return *m_tlasData;
}

void AccelerationStructureManager::cleanup() {
	if (m_blasScratcAllocation.has_value()) m_resourceManager.freeAllocation(*m_blasScratcAllocation);
	if (m_tlasScratchAllocation.has_value()) m_resourceManager.freeAllocation(*m_tlasScratchAllocation);
	if (m_instanceDataAllocation.has_value()) m_resourceManager.freeAllocation(*m_instanceDataAllocation);
}
