#include "scene/SceneManager.hpp"

#include <stdlib.h>

#include <array>
#include <cstddef>
#include <iostream>
#include <optional>
#include <vector>

#include "Common.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Primitive.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneLoader.hpp" #include "scene/SceneManager.hpp"
#include "scene/SceneLoader.hpp"

using SceneDataLocations = std::array<std::size_t, SceneLoader::SceneBuffersCount + 1>;

SceneManager::SceneManager(ResourceManager& resourceManager, MaterialManager& materialManager) :
	m_resourceManager(resourceManager), m_materialManager(materialManager) {
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
	dummyBuffers[(int)SceneManager::SceneBufferType::Transforms] = {
		.size = 1,
		.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eVertexBuffer,
	};
	dummyBuffers[(int)SceneManager::SceneBufferType::MaterialData] = {
		.size = 1,
		.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eStorageBuffer,
	};
	dummyBuffers[(int)SceneManager::SceneBufferType::PrimitiveData] = {
		.size = 1,
		.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eStorageBuffer,
	};

	m_dummyAllocation = m_resourceManager.createResources(
		{},
		std::vector<ResourceManager::BufferDescription>(dummyBuffers.begin(), dummyBuffers.end()),
		ResourceManager::MemoryLocation::Device
	);
	m_scene.allocation = m_dummyAllocation;
}

struct GeometryAllocationData {
	std::vector<ResourceManager::BufferDescription> buffers;
	std::array<MemorySpan, SceneLoader::SceneBuffersCount + 1> dataLocations;
};

GeometryAllocationData getBuffersInfo(const std::vector<SceneLoader::SceneInstance>& scenes) {
	std::array<MemorySpan, SceneLoader::SceneBuffersCount + 1> dataLocations;
	for (const auto& scene : scenes) {
		for (int i = 0; i < SceneLoader::SceneBuffersCount; i++) {
			dataLocations[i].size = scene.bufferDataLocations[i].size;
		}
		dataLocations[(int)SceneManager::SceneBufferType::PrimitiveData].size +=
			scene.primitives.size() * sizeof(Primitive::ShaderObject);
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
			.size = (uint32_t)dataLocations[(int)SceneManager::SceneBufferType::Transforms].size,
			.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
					 vk::BufferUsageFlagBits::eVertexBuffer,
		 },
		{
			.size = (uint32_t)dataLocations[(int)SceneManager::SceneBufferType::MaterialData].size,
			.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
					 vk::BufferUsageFlagBits::eStorageBuffer,
		 },
		{
			.size = (uint32_t)dataLocations[(int)SceneManager::SceneBufferType::PrimitiveData].size,
			.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
		 }
	};

	for (int i = 1; i < dataLocations.size(); i++) {
		dataLocations[i].offset = dataLocations[i - 1].offset + dataLocations[i - 1].size;
	}

	return {
		.buffers = buffers,
		.dataLocations = dataLocations,
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
	std::vector<ResourceManager::ResourceCopyInfo> copyInfo;
};
MergeInfo getMergeInfo(
	std::span<const BufferHandle> previousBuffers,
	std::span<const BufferHandle> newBuffers,
	const std::vector<SceneLoader::SceneInstance>& sceneData,
	std::size_t skipIndex
) {
	// Merge previous buffers to new buffers
	MergeInfo mergeInfo;
	for (int i = 0; i < skipIndex; i++) {
		for (int b = 0; b < SceneLoader::SceneBuffersCount; b++) {
			mergeInfo.buffersLayout[b].size += sceneData[i].bufferDataLocations[b].size;
			if (b > 0)
				mergeInfo.buffersLayout[b].offset =
					mergeInfo.buffersLayout[b - 1].offset + sceneData[i].bufferDataLocations[b - 1].size;
		}
	}

	for (int b = 0; b < SceneLoader::SceneBuffersCount; b++) {
		mergeInfo.copyInfo.push_back(
			{
				.source =
					ResourceManager::ResourceCopyInfo::BufferReference {
																		.handle = previousBuffers[b],
																		.size = (uint32_t)mergeInfo.buffersLayout[b].size,
																		.offset = (uint32_t)mergeInfo.buffersLayout[b].offset,
																		},

				.destination = ResourceManager::ResourceCopyInfo::BufferReference {
																		.handle = newBuffers[b],
																		.size = (uint32_t)mergeInfo.buffersLayout[b].size,
																		.offset = (uint32_t)mergeInfo.buffersLayout[b].offset,
																		}
		  }
		);
	}

	if (skipIndex < sceneData.size() - 1) {
		for (int b = 0; b < SceneLoader::SceneBuffersCount; b++) mergeInfo.buffersLayout[b].size = 0;

		for (int i = skipIndex + 1; i < sceneData.size(); i++) {
			for (int b = 0; b < SceneLoader::SceneBuffersCount; b++) {
				mergeInfo.buffersLayout[b].size += sceneData[i].bufferDataLocations[b].size;
				if (b > 1) mergeInfo.buffersLayout[b].offset += sceneData[i].bufferDataLocations[b - 1].size;
			}
		}

		for (int b = 0; b < SceneLoader::SceneBuffersCount; b++) {
			mergeInfo.copyInfo.push_back(
				{
					.source =
						ResourceManager::ResourceCopyInfo::BufferReference {
																			.handle = previousBuffers[b],
																			.size = (uint32_t)mergeInfo.buffersLayout[b].size,
																			.offset = (uint32_t)mergeInfo.buffersLayout[b].offset,
																			},

					.destination = ResourceManager::ResourceCopyInfo::BufferReference {
																			.handle = newBuffers[b],
																			.size = (uint32_t)mergeInfo.buffersLayout[b].size,
																			.offset = (uint32_t)mergeInfo.buffersLayout[b].offset,
																			}
			  }
			);
		}
	}

	return mergeInfo;
}

std::vector<ResourceManager::ResourceCopyInfo> getBuffersUploadInfo(
	BufferHandle stagingBuffer,
	const std::array<MemorySpan, SceneLoader::SceneBuffersCount>& stagingLayout,
	const std::array<MemorySpan, SceneLoader::SceneBuffersCount>& currentLayout,
	std::span<const BufferHandle> bufferHandles,
	std::size_t stagingOffset
) {
	std::vector<ResourceManager::ResourceCopyInfo> copies;
	copies.reserve(stagingLayout.size());

	for (int i = 0; i < stagingLayout.size(); i++) {
		copies.push_back(
			{
				.source =
					ResourceManager::ResourceCopyInfo::BufferReference {
																		.handle = stagingBuffer,
																		.size = static_cast<uint32_t>(stagingLayout[i].size),
																		.offset = static_cast<uint32_t>(stagingOffset + stagingLayout[i].offset),
																		},
				.destination = ResourceManager::ResourceCopyInfo::BufferReference {
																		.handle = bufferHandles[i],
																		.size = static_cast<uint32_t>(stagingLayout[i].size),
																		.offset = static_cast<uint32_t>(currentLayout[i].offset),

																		}
		  }
		);
	}
	return copies;
}

std::vector<ResourceManager::ResourceCopyInfo> getImagesUploadInfo(
	BufferHandle stagingBuffer,
	std::vector<std::size_t> images,
	const std::vector<MemorySpan>& dataLocations,
	const std::vector<glm::ivec2>& resolutions,
	std::span<const ImageHandle> handles
) {
	std::vector<ResourceManager::ResourceCopyInfo> copies;

	for (auto image : images) {
		std::size_t mipOffset = 0;
		std::size_t mipLevels = getMipLevels(resolutions[image].x, resolutions[image].y);
		for (int mip = 0; mip < mipLevels; mip++) {
			std::size_t mipSize = dataLocations[image].size >> (mip * 2);
			copies.push_back(
				{
					.source =
						ResourceManager::ResourceCopyInfo::BufferReference {
																			.handle = stagingBuffer,
																			.size = static_cast<uint32_t>(mipSize),
																			.offset = static_cast<uint32_t>(dataLocations[image].offset + mipOffset),
																			},
					.destination = ResourceManager::ResourceCopyInfo::ImageReference {
																			.handle = handles[image],
																			.mipLevel = static_cast<uint32_t>(mip),
																			.initialLayout = vk::ImageLayout::eUndefined,
																			.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
																			}
			  }
			);
			mipOffset += mipSize;
		}
	}

	return copies;
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
				.indexCount = primitive.indexCount,
			}
		);
	}
}

Scene mergeScenes(const std::vector<SceneLoader::SceneInstance>& scenes) {
	Scene scene;

	Offsets offsets;
	for (int i = 0; i < scenes.size(); i++) {
		auto& sceneInstance = scenes[i];

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
	}
	return scene;
}

SceneManager::ResourceCount SceneManager::loadAsync(const std::filesystem::path& path) {
	if (!std::filesystem::exists(path)) {
		std::cout << "Scene not found : " + path.relative_path().string() << std::endl;
		abort();
	}
	m_loadingData.emplace(path);

	m_scenes.push_back(m_loadingData->sceneLoader.getInstance());
	auto& scene = m_scenes.back();
	m_primitiveCount += scene.primitives.size();
	auto gpuBuffersInfo = getBuffersInfo(m_scenes);

	m_loadingData->stagingAllocation = m_resourceManager.createResources(
		{
	},
		{ {
			.size = static_cast<uint32_t>(
				scene.buffersStagingSize + scene.imageStagingSize + gpuBuffersInfo.dataLocations.back().size
			),
			.usage = vk::BufferUsageFlagBits::eTransferSrc,
		} },
		ResourceManager::MemoryLocation::Host
	);

	m_loadingData->newAllocation =
		m_resourceManager.createResources({}, gpuBuffersInfo.buffers, ResourceManager::MemoryLocation::Device);

	if (m_geometryAllocation) {
		auto mergeInfo = getMergeInfo(
			m_resourceManager.getBuffers(*m_geometryAllocation),
			m_resourceManager.getBuffers(m_loadingData->newAllocation),
			m_scenes,
			m_scenes.size() - 1
		);
		m_buffersLayout = mergeInfo.buffersLayout;
		m_resourceManager.copyResources(mergeInfo.copyInfo);
	}
	auto stagingBufferHandle = m_resourceManager.getBuffers(m_loadingData->stagingAllocation)[0];
	Buffer& stagingBuffer = m_resourceManager.getBuffer(stagingBufferHandle);

	auto imageDescriptions = getImageDescriptions(scene.imageResolution, scene.imageFormats);
	auto textureAllocation =
		m_resourceManager.createResources(imageDescriptions, {}, ResourceManager::MemoryLocation::Device);
	m_loadingData->sceneLoader.beginImageLoad(stagingBuffer.data);
	m_sceneTextureAllocations.push_back(textureAllocation);
	auto& allocationImages = m_resourceManager.getImages(textureAllocation);
	std::vector<ImageHandle> sceneImages(allocationImages.begin(), allocationImages.end());
	auto registeredImages = m_materialManager.registerTextureGroup(sceneImages);

	m_scene = mergeScenes(m_scenes);

	m_loadingData->sceneLoader.beginBufferLoad(
		(std::byte*)stagingBuffer.data + scene.imageStagingSize, registeredImages
	);
	loadPrimitiveData(
		(Primitive::ShaderObject*)((std::byte*)stagingBuffer.data + scene.buffersStagingSize + scene.imageStagingSize),
		m_scene
	);

	ResourceManager::ResourceCopyInfo primitiveDataCopy = {
		.source =
			ResourceManager::ResourceCopyInfo::BufferReference {
																.handle = stagingBufferHandle,
																.size = (uint32_t)gpuBuffersInfo.dataLocations[(int)SceneBufferType::PrimitiveData].size,
																.offset = (uint32_t)gpuBuffersInfo.dataLocations[(int)SceneBufferType::PrimitiveData].offset,
																},
		.destination =
			ResourceManager::ResourceCopyInfo::BufferReference {
																.handle =
					m_resourceManager.getBuffers(m_loadingData->newAllocation)[(int)SceneBufferType::PrimitiveData],
																.size = (uint32_t)gpuBuffersInfo.dataLocations[(int)SceneBufferType::PrimitiveData].size,
																.offset = 0,
																},
	};

	m_resourceManager.copyResources({ primitiveDataCopy });

	return scene.bufferDataLocations.size() + scene.imageDataLocations.size();
}

SceneManager::LoadedResourceCount SceneManager::sync() {
	auto delta = m_loadingData->sceneLoader.queryLoadStatus();
	std::size_t sceneIndex = m_scenes.size() - 1;
	auto& scene = m_scenes[sceneIndex];

	SceneManager::LoadedResourceCount count =
		m_loadingData->resourceLoadedCount + delta.loadedImages.size() + delta.loadedBuffers.size();

	m_loadingData->resourceLoadedCount = count;
	auto stagingBufferHandle = m_resourceManager.getBuffers(m_loadingData->stagingAllocation)[0];
	if (delta.loadedImages.size() > 0) {
		auto imageUploadInfo = getImagesUploadInfo(
			stagingBufferHandle,
			delta.loadedImages,
			scene.imageDataLocations,
			scene.imageResolution,
			m_resourceManager.getImages(m_sceneTextureAllocations[sceneIndex])
		);
		m_resourceManager.copyResources(imageUploadInfo);
	}
	if (delta.loadedBuffers.size() > 0) {
		auto bufferUploadInfo = getBuffersUploadInfo(
			stagingBufferHandle,
			scene.bufferDataLocations,
			m_buffersLayout,
			m_resourceManager.getBuffers(m_loadingData->newAllocation),
			scene.imageStagingSize
		);
		m_resourceManager.copyResources(bufferUploadInfo);
	}

	return count;
}

Scene SceneManager::getScene() {
	if (!m_loadingData) return m_scene;

	m_scenesGeometry.push_back(std::move(m_loadingData->sceneLoader).getSceneGeometry());

	for (auto& sceneGeometry : m_scenesGeometry) {
		m_scene.primitiveBounds.insert(
			m_scene.primitiveBounds.end(), sceneGeometry.primitiveBounds.begin(), sceneGeometry.primitiveBounds.end()
		);

		m_scene.size = std::max(m_scene.size, sceneGeometry.size);
	}

	if (m_geometryAllocation) m_resourceManager.freeAllocation(*m_geometryAllocation);
	m_resourceManager.freeAllocation(m_loadingData->stagingAllocation);
	m_geometryAllocation = m_loadingData->newAllocation;
	m_loadingData = std::nullopt;

	return m_scene;
}
