#include "scene/SceneManager.hpp"

#include <stdlib.h>

#include <array>
#include <cstddef>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "AccelerationStructureBuilder.hpp"
#include "Common.hpp"
#include "Instance.hpp"
#include "material/MaterialDefinitions.hpp"
#include "resources/ResourceManager.hpp"
#include "resources/ResourceWriteTransaction.hpp"
#include "scene/Primitive.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneLoader.hpp"
#include "scene/SceneManager.hpp"

struct StagingBufferHandles {
	const SceneManager::SharedBufferContainer<BufferHandle> sharedBuffers;
	std::span<const BufferHandle> images;

	StagingBufferHandles(const std::span<const BufferHandle> handles) :
		sharedBuffers(handles), images(handles.begin() + SceneLoader::SceneBuffersCount + 1, handles.end()) {}
};

vk::CommandPool createCommandPool(bool graphicQueue = false) {
	auto& instance = Instance::Get();
	return instance.device.createCommandPool(
		vk::CommandPoolCreateInfo {
			.flags = vk::CommandPoolCreateFlagBits::eTransient,
			.queueFamilyIndex = instance.queueFamiliesIndices.graphicsIndex,
		}
	);
}

void waitSemaphore(vk::Semaphore semaphore, uint64_t value) {
	auto& device = Instance::Get().device;
	auto waitResult = device.waitSemaphores(
		vk::SemaphoreWaitInfo {
			.semaphoreCount = 1,
			.pSemaphores = &semaphore,
			.pValues = &value,
		},
		UINT64_MAX
	);
	assert(waitResult == vk::Result::eSuccess);
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

	auto tempPool = createCommandPool();
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
	SceneManager::SharedBufferContainer<ResourceManager::BufferDescription> dummyBuffers;
	dummyBuffers.vertices = {
		.size = 1,
		.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eVertexBuffer |
				 vk::BufferUsageFlagBits::eShaderDeviceAddress,
	};
	dummyBuffers.vertexAttributes = {
		.size = 1,
		.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eVertexBuffer,
	};
	dummyBuffers.indices = {
		.size = 1,
		.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eIndexBuffer |
				 vk::BufferUsageFlagBits::eStorageBuffer,
	};
	dummyBuffers.transforms = {
		.size = 1,
		.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eVertexBuffer |
				 vk::BufferUsageFlagBits::eShaderDeviceAddress,
	};
	dummyBuffers.materialData = {
		.size = 1,
		.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eStorageBuffer,
	};
	dummyBuffers.primitiveData = {
		.size = 1,
		.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eStorageBuffer,
	};

	dummyBuffers.tlas = {
		.size = 1,
		.usage = vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR,
	};

	return resourceManager.createResources(
		{},
		std::vector<ResourceManager::BufferDescription>(dummyBuffers.values.begin(), dummyBuffers.values.end()),
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

static uint8_t getMipLevels(int width, int height) {
	uint32_t mipLevels = std::floor(std::log2(std::min(width, height))) + 1;
	mipLevels = mipLevels <= 3 ? 1 : mipLevels - 3;
	return mipLevels;
}

void mergeBuffers(
	ResourceWriteTransaction& transaction,
	std::span<const BufferHandle> previousBuffers,
	std::span<const BufferHandle> newBuffers,
	const std::vector<SceneLoader::SceneInstance>& sceneData,
	std::size_t skipIndex
) {
	SceneManager::SharedBufferContainer<std::size_t> bufferSizes;
	for (int i = 0; i < skipIndex; i++) {
		bufferSizes.vertices += sceneData[i].vertexCount * sizeof(Vertex);
		bufferSizes.vertexAttributes += sceneData[i].vertexCount * sizeof(Vertex::Attributes);
		bufferSizes.indices += sceneData[i].indexCount * sizeof(uint32_t);
		bufferSizes.transforms += sceneData[i].transforms.size() * sizeof(glm::mat4);
		bufferSizes.materialData += sceneData[i].materialCount * sizeof(MaterialDefinitions::PBRInstance);
	}

	for (int b = 0; b < SceneLoader::SceneBuffersCount; b++) {
		transaction.copy(
			ResourceWriteTransaction::BufferReference {
				.handle = previousBuffers[b],
				.size = (uint32_t)bufferSizes.values[b],
				.offset = 0,
			},
			ResourceWriteTransaction::BufferReference {
				.handle = newBuffers[b],
				.size = (uint32_t)bufferSizes.values[b],
				.offset = 0,
			}
		);
	}

	if (skipIndex < sceneData.size() - 1) {
		auto offsets = bufferSizes;

		offsets.vertices += sceneData[skipIndex].vertexCount * sizeof(Vertex);
		offsets.vertexAttributes += sceneData[skipIndex].vertexCount * sizeof(Vertex::Attributes);
		offsets.indices += sceneData[skipIndex].indexCount * sizeof(uint32_t);
		offsets.transforms += sceneData[skipIndex].transforms.size() * sizeof(glm::mat4);
		offsets.materialData += sceneData[skipIndex].materialCount * sizeof(MaterialDefinitions::PBRInstance);

		bufferSizes = {};

		for (int i = skipIndex + 1; i < sceneData.size(); i++) {
			bufferSizes.vertices += sceneData[i].vertexCount * sizeof(Vertex);
			bufferSizes.vertexAttributes += sceneData[i].vertexCount * sizeof(Vertex::Attributes);
			bufferSizes.indices += sceneData[i].indexCount * sizeof(uint32_t);
			bufferSizes.transforms += sceneData[i].transforms.size() * sizeof(glm::mat4);
			bufferSizes.materialData += sceneData[i].materialCount * sizeof(MaterialDefinitions::PBRInstance);
		}

		for (int b = 0; b < SceneLoader::SceneBuffersCount; b++) {
			transaction.copy(
				ResourceWriteTransaction::BufferReference {
					.handle = previousBuffers[b],
					.size = (uint32_t)bufferSizes.values[b],
					.offset = (uint32_t)offsets.values[b],
				},
				ResourceWriteTransaction::BufferReference {
					.handle = newBuffers[b],
					.size = (uint32_t)bufferSizes.values[b],
					.offset = (uint32_t)offsets.values[b],
				}
			);
		}
	}
}

void uploadBuffers(
	ResourceWriteTransaction& transaction,
	std::span<const BufferHandle> stagingBuffers,
	std::span<const BufferHandle> bufferHandles,
	std::span<const std::size_t> deviceBufferOffsets,
	ResourceManager& resourceManager
) {
	for (int i = 0; i < stagingBuffers.size(); i++) {
		auto size = resourceManager.getBuffer(stagingBuffers[i]).size;
		transaction.copy(
			ResourceWriteTransaction::BufferReference {
				.handle = stagingBuffers[i],
				.size = (uint32_t)size,
				.offset = 0,
			},
			ResourceWriteTransaction::BufferReference {
				.handle = bufferHandles[i],
				.size = (uint32_t)size,
				.offset = (uint32_t)deviceBufferOffsets[i],
			}
		);
	}
}

std::size_t getBaseImageSize(vk::Extent3D resolution, vk::Format format) {
	std::size_t alignedWidth = (resolution.width + 3) / 4 * 4;
	std::size_t alignedHeight = (resolution.height + 3) / 4 * 4;

	switch (format) {
		case vk::Format::eBc1RgbSrgbBlock:
		case vk::Format::eBc1RgbaSrgbBlock:
			return alignedWidth * alignedHeight / 16 * 8;
		case vk::Format::eBc5SnormBlock:
		case vk::Format::eBc5UnormBlock:
			return alignedWidth * alignedHeight / 16 * 16;

		default:
			return 0;
	}
}

void uploadImages(
	ResourceWriteTransaction& transaction,
	std::span<const std::size_t> images,
	std::span<const ImageHandle> handles,
	std::span<const BufferHandle> stagingBuffers,
	ResourceManager& resourceManager
) {
	for (auto imageIndex : images) {
		auto& image = resourceManager.getImage(handles[imageIndex]);

		std::size_t mipOffset = 0;
		std::size_t mipLevels = getMipLevels(image.size.width, image.size.height);
		std::size_t mipSize = getBaseImageSize(image.size, image.format);
		for (int mip = 0; mip < mipLevels; mip++) {
			transaction.copy(
				ResourceWriteTransaction::BufferReference {
					.handle = stagingBuffers[imageIndex],
					.size = static_cast<uint32_t>(mipSize),
					.offset = (uint32_t)mipOffset,
				},
				ResourceWriteTransaction::ImageReference {
					.handle = handles[imageIndex],
					.mipLevel = static_cast<uint32_t>(mip),
					.initialLayout = vk::ImageLayout::eUndefined,
					.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				}
			);
			mipOffset += mipSize;
			mipSize >>= 2;
		}
	}
}

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

Scene mergeSceneInstances(const std::vector<SceneLoader::SceneInstance>& scenes) {
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

		offsets.vertexOffset += sceneInstance.vertexCount;
		offsets.indexOffset += sceneInstance.indexCount;
		offsets.materialOffset += sceneInstance.materialCount;
	}
	return scene;
}
using SharedBuffersDescriptions = std::array<ResourceManager::BufferDescription, SceneLoader::SceneBuffersCount>;

struct Descriptions {
	std::vector<ResourceManager::BufferDescription> sharedBuffers;

	std::vector<ResourceManager::ImageDescription> sceneImages;
	std::vector<ResourceManager::BufferDescription> sceneBuffers;

	std::vector<ResourceManager::BufferDescription> stagingBuffers;
	std::vector<ResourceManager::BufferDescription> scratchBuffers;

	SharedBuffersDescriptions sharedBufferDescriptions;
};

Descriptions queryDescriptions(
	SceneLoader& loader,
	SceneLoader::SceneInstance& sceneInstance,
	Scene& mergedScene,
	std::span<const SharedBuffersDescriptions> sceneSharedBuffers
) {
	auto sceneAllocationInfo = loader.getAllocationInfo();

	std::vector<ResourceManager::BufferDescription> sharedBuffers;

	std::vector<ResourceManager::BufferDescription> sceneBuffers;
	std::vector<ResourceManager::ImageDescription> sceneImages;

	std::vector<ResourceManager::BufferDescription> stagingBuffers;
	std::vector<ResourceManager::BufferDescription> scratchBuffers;

	for (int i = 0; i < SceneLoader::SceneBuffersCount; i++) sharedBuffers.push_back({});

	for (const auto& sharedBufferDescription : sceneSharedBuffers) {
		for (int i = 0; i < SceneLoader::SceneBuffersCount; i++) {
			sharedBuffers[i].size += sharedBufferDescription[i].size;
			sharedBuffers[i].usage |= sharedBufferDescription[i].usage;
		}
	}
	for (int i = 0; i < SceneLoader::SceneBuffersCount; i++) {
		sharedBuffers[i].size += sceneAllocationInfo.geometryBuffers[i].size;
		sharedBuffers[i].usage |= sceneAllocationInfo.geometryBuffers[i].usage;
	}

	sharedBuffers.push_back(
		{
			.size = (uint32_t)(mergedScene.primitives.size() * sizeof(Primitive::ShaderObject)),
			.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
		}
	);

	sceneImages.insert(sceneImages.end(), sceneAllocationInfo.images.begin(), sceneAllocationInfo.images.end());

	stagingBuffers.insert(
		stagingBuffers.end(),
		sceneAllocationInfo.stagingSharedBuffers.begin(),
		sceneAllocationInfo.stagingSharedBuffers.end()
	);
	stagingBuffers.push_back(
		{
			.size = (uint32_t)(mergedScene.primitives.size() * sizeof(Primitive::ShaderObject)),
			.usage = vk::BufferUsageFlagBits::eTransferSrc,
		}
	);

	stagingBuffers.insert(
		stagingBuffers.end(),
		sceneAllocationInfo.stagingImageBuffers.begin(),
		sceneAllocationInfo.stagingImageBuffers.end()
	);

	AccelerationStructureBuilder asBuilder;
	auto blasAllocationInfo = asBuilder.queryBLASAllocationInfo(sceneInstance.primitives);
	sceneBuffers.insert(sceneBuffers.end(), blasAllocationInfo.asBuffers.begin(), blasAllocationInfo.asBuffers.end());

	scratchBuffers.insert(
		scratchBuffers.end(), blasAllocationInfo.scratchBuffers.begin(), blasAllocationInfo.scratchBuffers.end()
	);

	auto tlasAllocationInfo = asBuilder.queryTLASAllocationInfo(mergedScene.primitives.size());
	sharedBuffers.push_back(tlasAllocationInfo.asBuffer);
	scratchBuffers.push_back(tlasAllocationInfo.instanceData);
	scratchBuffers.push_back(tlasAllocationInfo.scratchBuffer);

	return {
		.sharedBuffers = std::move(sharedBuffers),
		.sceneImages = std::move(sceneImages),
		.sceneBuffers = std::move(sceneBuffers),
		.stagingBuffers = std::move(stagingBuffers),
		.scratchBuffers = std::move(scratchBuffers),
		.sharedBufferDescriptions = sceneAllocationInfo.geometryBuffers,
	};
}

void loadData(
	SceneLoader& sceneLoader,
	ResourceManager::AllocationIndex stagingAllocation,
	ResourceManager::AllocationIndex sceneAllocation,
	Scene& mergedScene,
	ResourceManager& resourceManager,
	MaterialManager& materialManager
) {
	std::vector<void*> imageAddresses;
	auto stagingBufferHandles = StagingBufferHandles(resourceManager.getBuffers(stagingAllocation));
	for (auto imageStagingHandle : stagingBufferHandles.images)
		imageAddresses.push_back(resourceManager.getBuffer(imageStagingHandle).data);
	sceneLoader.beginImageLoad(imageAddresses);

	auto& allocationImages = resourceManager.getImages(sceneAllocation);
	std::vector<ImageHandle> sceneImages(allocationImages.begin(), allocationImages.end());
	auto registeredImages = materialManager.registerTextureGroup(sceneImages);

	std::vector<void*> geometryBufferAddresses;
	geometryBufferAddresses.reserve(SceneLoader::SceneBuffersCount);
	for (auto& bufferStagingHandle : stagingBufferHandles.sharedBuffers.sceneLoaderBuffers())
		geometryBufferAddresses.push_back(resourceManager.getBuffer(bufferStagingHandle).data);
	sceneLoader.beginBufferLoad(geometryBufferAddresses, registeredImages);

	auto& primitiveStagingBuffer = resourceManager.getBuffer(stagingBufferHandles.sharedBuffers.primitiveData);
	auto* primitiveData = (Primitive::ShaderObject*)(primitiveStagingBuffer.data);
	for (int i = 0; i < mergedScene.primitives.size(); i++) {
		auto& primitive = mergedScene.primitives[i];
		primitiveData[i] = Primitive::ShaderObject {
			.baseVertex = primitive.baseVertex,
			.baseIndex = primitive.baseIndex,
			.materialIndex = primitive.materialIndex,
		};
	}
}

struct ASData {
	std::vector<vk::AccelerationStructureKHR> sceneBLAS;
	vk::AccelerationStructureKHR tlas;
};
ASData buildAS(
	ResourceWriteTransaction& transaction,
	SceneLoader::SceneInstance& scene,
	Scene& mergedScene,
	std::vector<vk::AccelerationStructureKHR>& mergedBLAS,
	ResourceManager::AllocationIndex sceneAllocation,
	ResourceManager::AllocationIndex scratchAllocation,
	ResourceManager::AllocationIndex sharedAllocation,
	ResourceManager& resourceManager
) {
	auto sharedBuffers =
		SceneManager::SharedBufferContainer<BufferHandle>(resourceManager.getBuffers(sharedAllocation));

	auto sceneAllocationHandles = resourceManager.getBuffers(sceneAllocation);
	auto scratchAllocationHandles = resourceManager.getBuffers(scratchAllocation);

	auto blasHandles = std::span<const BufferHandle>(
		sceneAllocationHandles.end() - scene.primitives.size(), sceneAllocationHandles.end()
	);

	auto blasScratchHandles = std::span<const BufferHandle>(
		scratchAllocationHandles.begin(), scratchAllocationHandles.begin() + scene.primitives.size()
	);

	AccelerationStructureBuilder builder;

	std::vector<vk::AccelerationStructureKHR> sceneBLAS;
	std::vector<vk::AccelerationStructureKHR> combinedBLAS(mergedBLAS);

	transaction.customOperation([&](vk::CommandBuffer& commandBuffer) {
		sceneBLAS = builder.buildBLAS(
			commandBuffer,
			{
				.primitives = scene.primitives,
				.blasBuffers = blasHandles,
				.scratchBuffers = blasScratchHandles,
				.vertexBuffer = sharedBuffers.vertices,
				.indexBuffer = sharedBuffers.indices,
				.resourceManager = resourceManager,
			}
		);

		combinedBLAS.insert(combinedBLAS.end(), sceneBLAS.begin(), sceneBLAS.end());
	});

	vk::AccelerationStructureKHR tlas;

	auto tlasInstanceHandle = scratchAllocationHandles.rbegin() + 1;
	auto tlasScratchHandle = scratchAllocationHandles.rbegin();

	transaction.customOperation([&](vk::CommandBuffer& commandBuffer) {
		tlas = builder.buildTLAS(
			commandBuffer,
			{
				.primitives = mergedScene.primitives,
				.blas = combinedBLAS,
				.transforms = mergedScene.transforms,
				.instancesBuffer = *tlasInstanceHandle,
				.asBuffer = sharedBuffers.tlas,
				.scratchBuffer = *tlasScratchHandle,
				.resourceManager = resourceManager,
			}
		);
	});
	return {
		.sceneBLAS = std::move(sceneBLAS),
		.tlas = tlas,
	};
}

SceneManager::ResourceCount SceneManager::loadAsync(const std::filesystem::path& path) {
	if (!std::filesystem::exists(path)) {
		std::cout << "Scene not found : " + path.relative_path().string() << std::endl;
		abort();
	}
	auto sceneLoader = std::make_unique<SceneLoader>(path);
	auto sceneInstance = sceneLoader->getSceneInstance();

	m_scenes.push_back(std::move(sceneInstance));
	auto mergedScene = mergeSceneInstances(m_scenes);

	auto commandPool = createCommandPool();

	auto descriptions = queryDescriptions(*sceneLoader, m_scenes.back(), mergedScene, m_sharedBuffersDescriptions);

	auto stagingAllocation =
		m_resourceManager.createResources({}, descriptions.stagingBuffers, ResourceManager::MemoryLocation::Host);

	auto sceneAllocation = m_resourceManager.createResources(
		descriptions.sceneImages, descriptions.sceneBuffers, ResourceManager::MemoryLocation::Device
	);

	auto sharedAllocation =
		m_resourceManager.createResources({}, descriptions.sharedBuffers, ResourceManager::MemoryLocation::Device);

	auto scratchAllocation = m_resourceManager.createResources(
		{}, descriptions.scratchBuffers, ResourceManager::MemoryLocation::HostVisible
	);

	loadData(*sceneLoader, stagingAllocation, sceneAllocation, mergedScene, m_resourceManager, m_materialManager);

	ResourceWriteTransaction transaction(commandPool, m_resourceManager);
	auto stagingBufferHandles = StagingBufferHandles(m_resourceManager.getBuffers(stagingAllocation));
	auto sharedBufferHandles = SharedBufferContainer<BufferHandle>(m_resourceManager.getBuffers(sharedAllocation));

	uploadBuffers(
		transaction,
		std::array { stagingBufferHandles.sharedBuffers.primitiveData },
		std::array { sharedBufferHandles.primitiveData },
		std::array { std::size_t(0) },
		m_resourceManager
	);

	if (m_sharedAllocation) {
		mergeBuffers(
			transaction,
			m_resourceManager.getBuffers(*m_sharedAllocation),
			m_resourceManager.getBuffers(sharedAllocation),
			m_scenes,
			m_scenes.size() - 1
		);
	}

	transaction.submit(Instance::Get().transferQueue, m_semaphore, ++m_transferCount);

	m_scenesAllocations.push_back(sceneAllocation);
	m_sharedBuffersDescriptions.push_back(descriptions.sharedBufferDescriptions);
	m_loadingData.emplace(
		LoadingData {
			.sceneLoader = std::move(sceneLoader),
			.mergedScene = mergedScene,
			.stagingAllocation = stagingAllocation,
			.sharedAllocation = sharedAllocation,
			.scratchAllocation = scratchAllocation,
			.commandPool = commandPool,
		}
	);

	return SceneLoader::SceneBuffersCount + m_resourceManager.getImages(sceneAllocation).size();
}

SceneManager::LoadedResourceCount SceneManager::sync() {
	auto delta = m_loadingData->sceneLoader->queryLoadStatus();
	std::size_t sceneIndex = m_scenes.size() - 1;

	SceneManager::LoadedResourceCount count =
		m_loadingData->resourceLoadedCount + delta.loadedImages.size() + delta.loadedBuffers.size();

	m_loadingData->resourceLoadedCount = count;
	auto stagingBufferHandles = StagingBufferHandles(m_resourceManager.getBuffers(m_loadingData->stagingAllocation));
	ResourceWriteTransaction transaction(m_loadingData->commandPool, m_resourceManager);

	if (delta.loadedImages.size() > 0) {
		uploadImages(
			transaction,
			delta.loadedImages,
			m_resourceManager.getImages(m_scenesAllocations[sceneIndex]),
			stagingBufferHandles.images,
			m_resourceManager
		);
	}
	if (delta.loadedBuffers.size() > 0) {
		SharedBufferContainer<std::size_t> offsets;
		auto sharedBuffers = m_resourceManager.getBuffers(m_loadingData->sharedAllocation);

		if (m_sharedAllocation.has_value()) {
			auto previousSharedBuffers = m_resourceManager.getBuffers(*m_sharedAllocation);
			for (int i = 0; i < offsets.Count; i++) {
				offsets.values[i] = m_resourceManager.getBuffer(previousSharedBuffers[i]).size;
			}
		}

		uploadBuffers(
			transaction,
			stagingBufferHandles.sharedBuffers.sceneLoaderBuffers(),
			sharedBuffers,
			offsets.values,
			m_resourceManager
		);

		std::vector<vk::AccelerationStructureKHR> blas;

		for (auto& sceneBLAS : m_scenesBLAS) blas.insert(blas.end(), sceneBLAS.begin(), sceneBLAS.end());

		auto asData = buildAS(
			transaction,
			m_scenes.back(),
			m_loadingData->mergedScene,
			blas,
			m_scenesAllocations.back(),
			m_loadingData->scratchAllocation,
			m_loadingData->sharedAllocation,
			m_resourceManager
		);
		m_scenesBLAS.push_back(std::move(asData.sceneBLAS));

		m_loadingData->mergedScene.tlasBuffer =
			SharedBufferContainer<BufferHandle>(m_resourceManager.getBuffers(m_loadingData->sharedAllocation)).tlas;
		m_loadingData->mergedScene.tlas = asData.tlas;
	}
	waitSemaphore(m_semaphore, m_transferCount);
	transaction.submit(Instance::Get().graphicQueue, m_semaphore, ++m_transferCount);

	return count;
}

Scene SceneManager::getScene() {
	if (!m_loadingData) return m_scene;

	waitSemaphore(m_semaphore, m_transferCount);

	m_scene = std::move(m_loadingData->mergedScene);
	m_scenesGeometry.push_back(m_loadingData->sceneLoader->getSceneGeometry());

	for (auto& sceneGeometry : m_scenesGeometry) {
		m_scene.primitiveBounds.insert(
			m_scene.primitiveBounds.end(), sceneGeometry.primitiveBounds.begin(), sceneGeometry.primitiveBounds.end()
		);

		m_scene.size = std::max(m_scene.size, sceneGeometry.size);
	}

	if (m_sharedAllocation) m_resourceManager.freeAllocation(*m_sharedAllocation);
	m_resourceManager.freeAllocation(m_loadingData->stagingAllocation);
	m_resourceManager.freeAllocation(m_loadingData->scratchAllocation);

	m_sharedAllocation = m_loadingData->sharedAllocation;
	m_scene.allocation = m_loadingData->sharedAllocation;
	Instance::Get().device.destroyCommandPool(m_loadingData->commandPool);
	m_loadingData = std::nullopt;

	return m_scene;
}
