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

template <typename T>
union SharedBufferContainer {
	static const std::size_t Count = SceneLoader::SceneBuffersCount + 1;
	std::array<T, SceneLoader::SceneBuffersCount + 1> values;

	struct {
		T vertices;
		T vertexAttributes;
		T indices;
		T transforms;
		T materialData;
		T primitiveData;
	};

	constexpr std::span<const T, SceneLoader::SceneBuffersCount> sceneLoaderBuffers() const {
		return std::span<const T, SceneLoader::SceneBuffersCount>(values.data(), SceneLoader::SceneBuffersCount);
	}
	SharedBufferContainer(const std::span<const T> pValues) {
		std::copy(pValues.begin(), pValues.begin() + Count, values.begin());
	}
	SharedBufferContainer() : values {} {}
};

struct StagingBufferHandles {
	const SharedBufferContainer<BufferHandle> sharedBuffers;
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
	SharedBufferContainer<ResourceManager::BufferDescription> dummyBuffers;
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
		.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eIndexBuffer,
	};
	dummyBuffers.transforms = {
		.size = 1,
		.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eIndexBuffer |
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
	m_scene.asAllocation = m_dummyAllocation;

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
	SharedBufferContainer<std::size_t> bufferSizes;
	for (int i = 0; i < skipIndex; i++) {
		bufferSizes.vertices += sceneData[i].vertexCount * sizeof(Vertex);
		bufferSizes.vertexAttributes += sceneData[i].vertexCount * sizeof(Vertex::Attributes);
		bufferSizes.indices += sceneData[i].indexCount * sizeof(uint32_t);
		bufferSizes.transforms += sceneData[i].transforms.size() * sizeof(glm::mat4);
		bufferSizes.materialData += sceneData[i].materialCount * sizeof(MaterialDefinitions::PBRInstance);
		bufferSizes.primitiveData += sceneData[i].primitives.size() * sizeof(Primitive::ShaderObject);
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
		offsets.primitiveData += sceneData[skipIndex].primitives.size() * sizeof(Primitive::ShaderObject);

		bufferSizes = {};

		for (int i = skipIndex + 1; i < sceneData.size(); i++) {
			bufferSizes.vertices += sceneData[i].vertexCount * sizeof(Vertex);
			bufferSizes.vertexAttributes += sceneData[i].vertexCount * sizeof(Vertex::Attributes);
			bufferSizes.indices += sceneData[i].indexCount * sizeof(uint32_t);
			bufferSizes.transforms += sceneData[i].transforms.size() * sizeof(glm::mat4);
			bufferSizes.materialData += sceneData[i].materialCount * sizeof(MaterialDefinitions::PBRInstance);
			bufferSizes.primitiveData += sceneData[i].primitives.size() * sizeof(Primitive::ShaderObject);
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
	SharedBuffersDescriptions sharedBufferDescriptions;
};

Descriptions queryDescriptions(
	SceneLoader& loader, Scene& mergedScene, std::span<const SharedBuffersDescriptions> sceneSharedBuffers
) {
	auto sceneAllocationInfo = loader.getAllocationInfo();

	std::vector<ResourceManager::BufferDescription> sharedBuffers;

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

	std::vector<ResourceManager::ImageDescription> sceneImages;
	sceneImages.insert(sceneImages.end(), sceneAllocationInfo.images.begin(), sceneAllocationInfo.images.end());

	std::vector<ResourceManager::BufferDescription> stagingBuffers;
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

	return {
		.sharedBuffers = std::move(sharedBuffers),
		.sceneImages = std::move(sceneImages),
		.sceneBuffers = {},
		.stagingBuffers = std::move(stagingBuffers),
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
				.vertexBuffer = resourceManager.getBuffer(bufferHandles[(int)SceneManager::SceneBufferType::Vertex]),
				.indexBuffer = resourceManager.getBuffer(bufferHandles[(int)SceneManager::SceneBufferType::Index]),
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
	return tlasData;
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

	auto descriptions = queryDescriptions(*sceneLoader, mergedScene, m_sharedBuffersDescriptions);

	auto stagingAllocation =
		m_resourceManager.createResources({}, descriptions.stagingBuffers, ResourceManager::MemoryLocation::Host);

	auto sceneAllocation = m_resourceManager.createResources(
		descriptions.sceneImages, descriptions.sceneBuffers, ResourceManager::MemoryLocation::Device
	);

	auto sharedAllocation =
		m_resourceManager.createResources({}, descriptions.sharedBuffers, ResourceManager::MemoryLocation::Device);

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

	m_sceneTextureAllocations.push_back(sceneAllocation);
	m_sharedBuffersDescriptions.push_back(descriptions.sharedBufferDescriptions);
	m_loadingData.emplace(
		LoadingData {
			.sceneLoader = std::move(sceneLoader),
			.mergedScene = mergedScene,
			.stagingAllocation = stagingAllocation,
			.sharedAllocation = sharedAllocation,
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
			m_resourceManager.getImages(m_sceneTextureAllocations[sceneIndex]),
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
		AccelerationStructureBuilder asBuider(m_resourceManager);

		auto asData = buildAS(transaction, asBuider, m_resourceManager, m_loadingData->scene);
		m_loadingData->scene.asAllocation = asData.allocation;
		m_loadingData->scene.tlas = asData.tlas;
	}

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
	m_sharedAllocation = m_loadingData->sharedAllocation;
	m_scene.allocation = *m_sharedAllocation;
	Instance::Get().device.destroyCommandPool(m_loadingData->commandPool);
	m_loadingData = std::nullopt;

	return m_scene;
}
