#include "scene/SceneManager.hpp"

#include <stdlib.h>

#include <array>
#include <cstddef>
#include <iostream>
#include <optional>
#include <vector>

#include "Common.hpp"
#include "Instance.hpp"
#include "material/MaterialDefinitions.hpp"
#include "resources/ResourceManager.hpp"
#include "resources/ResourceWriteTransaction.hpp"
#include "scene/AccelerationStructureBuilder.hpp"
#include "scene/Primitive.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneLoader.hpp"
#include "scene/SceneManager.hpp"

vk::CommandPool createCommandPool(bool graphicQueue = false) {
	auto& instance = Instance::Get();
	return instance.device.createCommandPool(
		vk::CommandPoolCreateInfo {
			.flags = vk::CommandPoolCreateFlagBits::eTransient,
			.queueFamilyIndex = graphicQueue ? instance.queueFamiliesIndices.graphicsIndex
											 : instance.queueFamiliesIndices.transferIndex,
		}
	);
}

void waitSemaphore(vk::Semaphore semaphore, uint64_t value) {
	auto& device = Instance::Get().device;
	assert(
		device.waitSemaphores(
			vk::SemaphoreWaitInfo {
				.semaphoreCount = 1,
				.pSemaphores = &semaphore,
				.pValues = &value,
			},
			UINT64_MAX
		) == vk::Result::eSuccess
	);
}

void createPlaceholderTextures(
	ResourceManager& resourceManager, MaterialManager& materialManager, vk::Semaphore& semaphore, uint64_t signalValue
) {
	std::vector<ResourceManager::ImageDescription> descriptions(3);
	descriptions[0] = {
		.width = 1,
		.height = 1,
		.depth = 1,
		.miplevels = 1,
		.format = vk::Format::eR8G8B8A8Unorm,
		.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
	};
	descriptions[1] = {
		.width = 1,
		.height = 1,
		.depth = 1,
		.miplevels = 1,
		.format = vk::Format::eR8G8B8A8Unorm,
		.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,

	};
	descriptions[2] = {
		.width = 1,
		.height = 1,
		.depth = 1,
		.miplevels = 1,
		.format = vk::Format::eR8G8B8A8Unorm,
		.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
	};

	auto allocationIndex =
		resourceManager.createResources({ descriptions }, {}, ResourceManager::MemoryLocation::Device);
	auto images = resourceManager.getImages(allocationIndex);
	materialManager.registerTextureGroup(std::vector<ImageHandle>(images.begin(), images.end()));

	auto tempPool = createCommandPool(true);
	ResourceWriteTransaction transaction(tempPool, resourceManager);
	transaction.imageClear(
		{
			.handle = images[0],
			.mipLevel = 0,
			.initialLayout = vk::ImageLayout::eUndefined,
			.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
	},
		vk::ClearColorValue { .float32 = std::array<float, 4> { 1.0f, 1.0f, 1.0f, 1.0f } }
	);

	transaction.imageClear(
		{
			.handle = images[1],
			.mipLevel = 0,
			.initialLayout = vk::ImageLayout::eUndefined,
			.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
	},
		vk::ClearColorValue { .float32 = std::array<float, 4> { 0.5f, 0.5f, 1.0f, 1.0f } }
	);

	transaction.imageClear(
		{
			.handle = images[2],
			.mipLevel = 0,
			.initialLayout = vk::ImageLayout::eUndefined,
			.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
	},
		vk::ClearColorValue { .float32 = std::array<float, 4> { 1.0f, 0.0f, 0.0f, 1.0f } }
	);
	auto& instance = Instance::Get();
	transaction.submit(instance.graphicQueue, semaphore, signalValue);

	waitSemaphore(semaphore, signalValue);
	instance.device.destroyCommandPool(tempPool);
}

ResourceManager::AllocationIndex createDummyAllocation(ResourceManager& resourceManager) {
	std::array<MemorySpan, SceneLoader::SceneBuffersCount> dummyLocations;

	for (int i = 0; i < SceneLoader::SceneBuffersCount; i++) {
		dummyLocations[i] = { .size = 1, .offset = (std::size_t)i };
	}

	std::array<ResourceManager::BufferDescription, SceneLoader::SceneBuffersCount + 1> dummyBuffers;
	dummyBuffers[(int)SceneManager::SceneBufferType::Vertex] = {
		.size = 1,
		.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eVertexBuffer,
	};
	dummyBuffers[(int)SceneManager::SceneBufferType::VertexAttribute] = {
		.size = 1,
		.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eVertexBuffer,
	};
	dummyBuffers[(int)SceneManager::SceneBufferType::Index] = {
		.size = 1,
		.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eIndexBuffer,
	};
	dummyBuffers[(int)SceneManager::SceneBufferType::MaterialData] = {
		.size = 1,
		.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eStorageBuffer,
	};
	dummyBuffers[(int)SceneManager::SceneBufferType::Transforms] = {
		.size = 1,
		.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eVertexBuffer,
	};
	dummyBuffers[(int)SceneManager::SceneBufferType::PrimitiveData] = {
		.size = 1,
		.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eStorageBuffer,
	};

	return resourceManager.createResources(
		{},
		std::vector<ResourceManager::BufferDescription>(dummyBuffers.begin(), dummyBuffers.end()),
		ResourceManager::MemoryLocation::Device
	);
}

vk::Semaphore createSemaphore() {
	vk::SemaphoreTypeCreateInfo info {
		.semaphoreType = vk::SemaphoreType::eTimeline,
		.initialValue = 0,
	};

	return Instance::Get().device.createSemaphore({ .pNext = &info });
}

SceneManager::SceneManager(ResourceManager& resourceManager, MaterialManager& materialManager) :
	m_resourceManager(resourceManager), m_materialManager(materialManager) {
	m_semaphore = createSemaphore();
	m_dummyAllocation = createDummyAllocation(resourceManager);
	m_scene.allocation = m_dummyAllocation;

	createPlaceholderTextures(resourceManager, materialManager, m_semaphore, ++m_transferCount);
}

struct GeometryAllocationData {
	std::vector<ResourceManager::BufferDescription> buffers;
	std::array<MemorySpan, SceneLoader::SceneBuffersCount> dataLocations;
	std::size_t primitiveDataSize;
	std::size_t transformDataSize;
};

GeometryAllocationData getBuffersInfo(const std::vector<SceneLoader::SceneInstance>& scenes) {
	std::array<MemorySpan, SceneLoader::SceneBuffersCount> dataLocations;
	std::size_t primitiveDataSize = 0;
	std::size_t transformsDataSize = 0;

	for (const auto& scene : scenes) {
		for (int i = 0; i < SceneLoader::SceneBuffersCount; i++) {
			dataLocations[i].size = scene.bufferDataLocations[i].size;
		}
		transformsDataSize += scene.transforms.size() * sizeof(glm::mat4);
		primitiveDataSize += scene.primitives.size() * sizeof(Primitive::ShaderObject);
	}

	std::vector<ResourceManager::BufferDescription> buffers {
		{
			.size = (uint32_t)dataLocations[(int)SceneManager::SceneBufferType::Vertex].size,
			.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
					 vk::BufferUsageFlagBits::eVertexBuffer,
		 },
		{
			.size = (uint32_t)dataLocations[(int)SceneManager::SceneBufferType::VertexAttribute].size,
			.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
					 vk::BufferUsageFlagBits::eVertexBuffer,
		 },
		{
			.size = (uint32_t)dataLocations[(int)SceneManager::SceneBufferType::Index].size,
			.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
					 vk::BufferUsageFlagBits::eIndexBuffer,
		 },
		{
			.size = (uint32_t)dataLocations[(int)SceneManager::SceneBufferType::MaterialData].size,
			.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
					 vk::BufferUsageFlagBits::eStorageBuffer,
		 },
		{
			.size = (uint32_t)transformsDataSize,
			.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
					 vk::BufferUsageFlagBits::eVertexBuffer,
		 },
		{
			.size = (uint32_t)primitiveDataSize,
			.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
		 }
	};

	for (int i = 1; i < dataLocations.size(); i++) {
		dataLocations[i].offset = dataLocations[i - 1].offset + dataLocations[i - 1].size;
	}
	return {
		.buffers = buffers,
		.dataLocations = dataLocations,
		.primitiveDataSize = primitiveDataSize,
		.transformDataSize = transformsDataSize,
	};
}

static uint8_t getMipLevels(int width, int height) {
	uint32_t mipLevels = std::floor(std::log2(std::min(width, height))) + 1;
	mipLevels = mipLevels <= 3 ? 1 : mipLevels - 3;
	return mipLevels;
}

std::vector<ResourceManager::ImageDescription> getImageDescriptions(
	const std::vector<glm::ivec2>& resolution, const std::vector<vk::Format>& formats
) {
	std::vector<ResourceManager::ImageDescription> descriptions;

	for (int i = 0; i < resolution.size(); i++) {
		descriptions.push_back(
			{
				.width = (uint32_t)resolution[i].x,
				.height = (uint32_t)resolution[i].y,
				.depth = 1,
				.miplevels = getMipLevels(resolution[i].x, resolution[i].y),
				.format = formats[i],
				.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,

			}
		);
	}

	return descriptions;
}

struct MergeInfo {
	std::array<MemorySpan, SceneLoader::SceneBuffersCount> buffersLayout;
};

MergeInfo mergeBuffers(
	ResourceWriteTransaction& transaction,
	std::span<const BufferHandle> previousBuffers,
	std::span<const BufferHandle> newBuffers,
	const std::vector<SceneLoader::SceneInstance>& sceneData,
	std::size_t skipIndex
) {
	std::array<MemorySpan, SceneLoader::SceneBuffersCount> buffersLayout;
	for (int i = 0; i < skipIndex; i++) {
		for (int b = 0; b < SceneLoader::SceneBuffersCount; b++) {
			buffersLayout[b].size += sceneData[i].bufferDataLocations[b].size;
			if (b > 0)
				buffersLayout[b].offset = buffersLayout[b - 1].offset + sceneData[i].bufferDataLocations[b - 1].size;
		}
	}

	for (int b = 0; b < SceneLoader::SceneBuffersCount; b++) {
		transaction.copy(
			ResourceWriteTransaction::BufferReference {
				.handle = previousBuffers[b],
				.size = (uint32_t)buffersLayout[b].size,
				.offset = (uint32_t)buffersLayout[b].offset,
			},
			ResourceWriteTransaction::BufferReference {
				.handle = newBuffers[b],
				.size = (uint32_t)buffersLayout[b].size,
				.offset = (uint32_t)buffersLayout[b].offset,
			}
		);
	}

	if (skipIndex < sceneData.size() - 1) {
		for (int b = 0; b < SceneLoader::SceneBuffersCount; b++) buffersLayout[b].size = 0;

		for (int i = skipIndex + 1; i < sceneData.size(); i++) {
			for (int b = 0; b < SceneLoader::SceneBuffersCount; b++) {
				buffersLayout[b].size += sceneData[i].bufferDataLocations[b].size;
				if (b > 1) buffersLayout[b].offset += sceneData[i].bufferDataLocations[b - 1].size;
			}
		}

		for (int b = 0; b < SceneLoader::SceneBuffersCount; b++) {
			transaction.copy(
				ResourceWriteTransaction::BufferReference {
					.handle = previousBuffers[b],
					.size = (uint32_t)buffersLayout[b].size,
					.offset = (uint32_t)buffersLayout[b].offset,
				},
				ResourceWriteTransaction::BufferReference {
					.handle = newBuffers[b],
					.size = (uint32_t)buffersLayout[b].size,
					.offset = (uint32_t)buffersLayout[b].offset,
				}
			);
		}
	}

	return { buffersLayout };
}

void uploadBuffers(
	ResourceWriteTransaction& transaction,
	BufferHandle stagingBuffer,
	const std::array<MemorySpan, SceneLoader::SceneBuffersCount>& stagingLayout,
	const std::array<MemorySpan, SceneLoader::SceneBuffersCount>& currentLayout,
	std::span<const BufferHandle> bufferHandles,
	std::size_t stagingOffset
) {
	for (int i = 0; i < stagingLayout.size(); i++) {
		transaction.copy(
			ResourceWriteTransaction::BufferReference {
				.handle = stagingBuffer,
				.size = static_cast<uint32_t>(stagingLayout[i].size),
				.offset = static_cast<uint32_t>(stagingOffset + stagingLayout[i].offset),
			},
			ResourceWriteTransaction::BufferReference {
				.handle = bufferHandles[i],
				.size = static_cast<uint32_t>(stagingLayout[i].size),
				.offset = static_cast<uint32_t>(currentLayout[i].offset),
			}
		);
	}
}

void uploadImages(
	ResourceWriteTransaction& transaction,
	BufferHandle stagingBuffer,
	std::vector<std::size_t> images,
	const std::vector<MemorySpan>& dataLocations,
	const std::vector<glm::ivec2>& resolutions,
	std::span<const ImageHandle> handles
) {
	for (auto image : images) {
		std::size_t mipOffset = 0;
		std::size_t mipLevels = getMipLevels(resolutions[image].x, resolutions[image].y);
		for (int mip = 0; mip < mipLevels; mip++) {
			std::size_t mipSize = dataLocations[image].size >> (mip * 2);
			transaction.copy(
				ResourceWriteTransaction::BufferReference {
					.handle = stagingBuffer,
					.size = static_cast<uint32_t>(mipSize),
					.offset = static_cast<uint32_t>(dataLocations[image].offset + mipOffset),
				},
				ResourceWriteTransaction::ImageReference {
					.handle = handles[image],
					.mipLevel = static_cast<uint32_t>(mip),
					.initialLayout = vk::ImageLayout::eUndefined,
					.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				}
			);
			mipOffset += mipSize;
		}
	}
}

void loadPrimitiveData(Primitive::ShaderObject* primitiveData, const Scene& scene) {
	for (int i = 0; i < scene.primitives.size(); i++) {
		auto& primitive = scene.primitives[i];
		primitiveData[i] = Primitive::ShaderObject {
			.baseVertex = primitive.baseVertex,
			.baseIndex = primitive.baseIndex,
			.materialIndex = primitive.materialIndex,
		};
	}
};

struct Offsets {
	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;
	uint32_t materialOffset = 0;
	uint32_t tranformOffset = 0;
};
void composePrimitives(
	const std::vector<Primitive>& scenePrimitives, std::vector<Primitive>& outputPrimitives, Offsets baseOffsets
) {
	outputPrimitives.reserve(outputPrimitives.size() + scenePrimitives.size());
	for (const auto& primitive : scenePrimitives) {
		outputPrimitives.push_back(
			{
				.baseVertex = primitive.baseVertex + baseOffsets.vertexOffset,
				.baseIndex = primitive.baseIndex + baseOffsets.indexOffset,
				.vertexCount = primitive.vertexCount,
				.indexCount = primitive.indexCount,
				.materialIndex = primitive.materialIndex + baseOffsets.materialOffset,
				.baseTransform = baseOffsets.tranformOffset,
			}
		);
	}
}

Scene mergeScenes(const std::vector<SceneLoader::SceneInstance>& scenes) {
	Scene scene;

	Offsets offsets;
	for (int i = 0; i < scenes.size(); i++) {
		auto& sceneInstance = scenes[i];

		scene.transforms.insert(
			scene.transforms.end(), sceneInstance.transforms.begin(), sceneInstance.transforms.end()
		);

		scene.materialHints.insert(
			scene.materialHints.end(), sceneInstance.materialHints.begin(), sceneInstance.materialHints.end()
		);

		composePrimitives(sceneInstance.primitives, scene.primitives, offsets);

		std::size_t vertexCount =
			sceneInstance.bufferDataLocations[(int)SceneManager::SceneBufferType::Vertex].size / sizeof(glm::vec3);
		std::size_t indexCount =
			sceneInstance.bufferDataLocations[(int)SceneManager::SceneBufferType::Index].size / sizeof(uint32_t);
		std::size_t materialCount =
			sceneInstance.bufferDataLocations[(int)SceneManager::SceneBufferType::MaterialData].size /
			sizeof(MaterialDefinitions::PBRInstance);

		offsets.vertexOffset += vertexCount;
		offsets.indexOffset += indexCount;
		offsets.materialOffset += materialCount;
		offsets.tranformOffset += sceneInstance.transforms.size();
	}
	return scene;
}

AccelerationStructureBuilder::TLASData buildAS(
	ResourceWriteTransaction& transaction,
	AccelerationStructureBuilder& builder,
	ResourceManager& resourceManager,
	Scene& mergedScene
) {
	auto bufferHandles = resourceManager.getBuffers(mergedScene.allocation);

	AccelerationStructureBuilder::BLASData blasData;
	transaction.customOperation([&](vk::CommandBuffer& commandBuffer) {
		blasData = builder.buildBLAS(
			commandBuffer,
			{
				.primitives = mergedScene.primitives,
				.vertexBuffer =
					resourceManager.getBuffer(bufferHandles[(int)SceneManager::SceneBufferType::Vertex]).buffer,
				.indexBuffer =
					resourceManager.getBuffer(bufferHandles[(int)SceneManager::SceneBufferType::Index]).buffer,
			}
		);
	});
	AccelerationStructureBuilder::TLASData tlasData;

	transaction.customOperation([&](vk::CommandBuffer& commandBuffer) {
		tlasData = builder.buildTLAS(
			commandBuffer,
			{
				.primitives = mergedScene.primitives,
				.blas = blasData.blas,
				.transforms = mergedScene.transforms,
			}
		);
	});
}

SceneManager::ResourceCount SceneManager::loadAsync(const std::filesystem::path& path) {
	if (!std::filesystem::exists(path)) {
		std::cout << "Scene not found : " + path.relative_path().string() << std::endl;
		abort();
	}

	m_loadingData = { .sceneLoader = SceneLoader(path) };

	m_scenes.push_back(m_loadingData->sceneLoader.getInstance());
	auto& sceneInstance = m_scenes.back();
	m_primitiveCount += sceneInstance.primitives.size();
	auto gpuBuffersInfo = getBuffersInfo(m_scenes);

	auto stagingAllocation = m_resourceManager.createResources(
		{
	},
		{ {
			.size = static_cast<uint32_t>(
				sceneInstance.buffersStagingSize + sceneInstance.imageStagingSize + gpuBuffersInfo.primitiveDataSize
			),
			.usage = vk::BufferUsageFlagBits::eTransferSrc,
		} },
		ResourceManager::MemoryLocation::Host
	);

	auto deviceAllocation =
		m_resourceManager.createResources({}, gpuBuffersInfo.buffers, ResourceManager::MemoryLocation::Device);
	auto commandPool = createCommandPool();
	ResourceWriteTransaction transaction(commandPool, m_resourceManager);

	if (m_sceneAllocation) {
		auto mergeInfo = mergeBuffers(
			transaction,
			m_resourceManager.getBuffers(*m_sceneAllocation),
			m_resourceManager.getBuffers(deviceAllocation),
			m_scenes,
			m_scenes.size() - 1
		);
		m_buffersLayout = mergeInfo.buffersLayout;
	}
	auto stagingBufferHandle = m_resourceManager.getBuffers(stagingAllocation)[0];
	Buffer& stagingBuffer = m_resourceManager.getBuffer(stagingBufferHandle);

	auto imageDescriptions = getImageDescriptions(sceneInstance.imageResolution, sceneInstance.imageFormats);
	auto textureAllocation =
		m_resourceManager.createResources(imageDescriptions, {}, ResourceManager::MemoryLocation::Device);
	m_loadingData->sceneLoader.beginImageLoad(stagingBuffer.data);
	m_sceneTextureAllocations.push_back(textureAllocation);
	auto& allocationImages = m_resourceManager.getImages(textureAllocation);
	std::vector<ImageHandle> sceneImages(allocationImages.begin(), allocationImages.end());
	auto registeredImages = m_materialManager.registerTextureGroup(sceneImages);

	auto mergedScene = mergeScenes(m_scenes);

	std::size_t sceneLoaderImageOffset = 0;
	std::size_t sceneLoaderBufferOffset = sceneLoaderImageOffset + sceneInstance.imageStagingSize;
	std::size_t primitiveDataOffset = sceneLoaderBufferOffset + sceneInstance.buffersStagingSize;
	std::size_t transformsDataOffset = primitiveDataOffset + gpuBuffersInfo.primitiveDataSize;

	m_loadingData->sceneLoader.beginBufferLoad(
		(std::byte*)stagingBuffer.data + sceneLoaderBufferOffset, registeredImages
	);
	loadPrimitiveData((Primitive::ShaderObject*)((std::byte*)stagingBuffer.data + primitiveDataOffset), mergedScene);
	transaction.copy(
		ResourceWriteTransaction::BufferReference {
			.handle = stagingBufferHandle,
			.size = (uint32_t)gpuBuffersInfo.primitiveDataSize,
			.offset = (uint32_t)(primitiveDataOffset),
		},
		ResourceWriteTransaction::BufferReference {
			.handle = m_resourceManager.getBuffers(deviceAllocation)[(int)SceneBufferType::PrimitiveData],
			.size = (uint32_t)gpuBuffersInfo.primitiveDataSize,
			.offset = 0,
		}
	);

	memcpy(
		(std::byte*)stagingBuffer.data + transformsDataOffset,
		mergedScene.transforms.data(),
		gpuBuffersInfo.primitiveDataSize
	);

	transaction.copy(
		ResourceWriteTransaction::BufferReference {
			.handle = stagingBufferHandle,
			.size = (uint32_t)gpuBuffersInfo.primitiveDataSize,
			.offset = (uint32_t)(transformsDataOffset),
		},
		ResourceWriteTransaction::BufferReference {
			.handle = m_resourceManager.getBuffers(deviceAllocation)[(int)SceneBufferType::Transforms],
			.size = (uint32_t)gpuBuffersInfo.transformDataSize,
			.offset = 0,
		}
	);

	transaction.submit(Instance::Get().transferQueue, m_semaphore, ++m_transferCount);

	m_loadingData->commandPool = commandPool;
	m_loadingData->stagingAllocation = stagingAllocation;
	m_loadingData->newAllocation = deviceAllocation;
	m_loadingData->scene = mergedScene;

	return sceneInstance.bufferDataLocations.size() + sceneInstance.imageDataLocations.size();
}

SceneManager::LoadedResourceCount SceneManager::sync() {
	auto delta = m_loadingData->sceneLoader.queryLoadStatus();
	std::size_t sceneIndex = m_scenes.size() - 1;
	auto& scene = m_scenes[sceneIndex];

	SceneManager::LoadedResourceCount count =
		m_loadingData->resourceLoadedCount + delta.loadedImages.size() + delta.loadedBuffers.size();

	m_loadingData->resourceLoadedCount = count;
	auto stagingBufferHandle = m_resourceManager.getBuffers(m_loadingData->stagingAllocation)[0];

	ResourceWriteTransaction transaction(m_loadingData->commandPool, m_resourceManager);

	if (delta.loadedImages.size() > 0) {
		uploadImages(
			transaction,
			stagingBufferHandle,
			delta.loadedImages,
			scene.imageDataLocations,
			scene.imageResolution,
			m_resourceManager.getImages(m_sceneTextureAllocations[sceneIndex])
		);
	}
	if (delta.loadedBuffers.size() > 0) {
		uploadBuffers(
			transaction,
			stagingBufferHandle,
			scene.bufferDataLocations,
			m_buffersLayout,
			m_resourceManager.getBuffers(m_loadingData->newAllocation),
			scene.imageStagingSize
		);
	}

	transaction.submit(Instance::Get().transferQueue, m_semaphore, ++m_transferCount);

	return count;
}

Scene SceneManager::getScene() {
	if (!m_loadingData) return m_scene;

	waitSemaphore(m_semaphore, m_transferCount);

	m_scene = std::move(m_loadingData->scene);
	m_scenesGeometry.push_back(std::move(m_loadingData->sceneLoader).getSceneGeometry());

	for (auto& sceneGeometry : m_scenesGeometry) {
		m_scene.primitiveBounds.insert(
			m_scene.primitiveBounds.end(), sceneGeometry.primitiveBounds.begin(), sceneGeometry.primitiveBounds.end()
		);

		m_scene.size = std::max(m_scene.size, sceneGeometry.size);
	}

	if (m_sceneAllocation) m_resourceManager.freeAllocation(*m_sceneAllocation);
	m_resourceManager.freeAllocation(m_loadingData->stagingAllocation);
	m_sceneAllocation = m_loadingData->newAllocation;
	m_scene.allocation = *m_sceneAllocation;
	Instance::Get().device.destroyCommandPool(m_loadingData->commandPool);
	m_loadingData = std::nullopt;

	return m_scene;
}
