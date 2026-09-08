#include "scene/SceneManager.hpp"

#include <stdlib.h>

#include <array>
#include <cstddef>
#include <iostream>
#include <optional>
#include <vector>

#include "Common.hpp"
#include "Instance.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Primitive.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneLoader.hpp" #include "scene/SceneManager.hpp"
#include "scene/SceneLoader.hpp"

struct GeometryAllocationData {
	std::vector<ResourceManager::BufferDescription> buffers;
	std::array<std::size_t, 6> offsets;
};

GeometryAllocationData getGeometryAllocationData(const std::vector<SceneLoader::SceneResources>& sceneData) {
	std::array<uint32_t, 6> sizes { 0 };

	for (const auto& scene : sceneData) {
		for (int i = 0; i < 6; i++) {
			sizes[i] = scene.bufferDataLocations[i].size;
		}
	}

	std::vector<ResourceManager::BufferDescription> buffers {
		{
			.size = sizes[SceneManager::GeometryBufferType::Vertex],
			.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
					 vk::BufferUsageFlagBits::eVertexBuffer,
		 },
		{
			.size = sizes[SceneManager::GeometryBufferType::VertexAttribute],
			.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
					 vk::BufferUsageFlagBits::eVertexBuffer,
		 },
		{
			.size = sizes[SceneManager::GeometryBufferType::Index],
			.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
					 vk::BufferUsageFlagBits::eIndexBuffer,
		 },
		{
			.size = sizes[SceneManager::GeometryBufferType::Transforms],
			.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
					 vk::BufferUsageFlagBits::eVertexBuffer,
		 },
		{
			.size = sizes[SceneManager::GeometryBufferType::MaterialInstances],
			.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
					 vk::BufferUsageFlagBits::eStorageBuffer,
		 },
		{
			.size = sizes[SceneManager::GeometryBufferType::Materials],
			.usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst |
					 vk::BufferUsageFlagBits::eStorageBuffer,
		 }
	};

	std::array<std::size_t, 6> offsets { 0 };

	for (int i = 1; i < offsets.size(); i++) {
		offsets[i] = offsets[i - 1] + sizes[i];
	}

	return {
		.buffers = buffers,
		.offsets = offsets,
	};
}

uint8_t getMipLevels(int width, int height) {
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
	std::array<MemorySpan, 6> buffersLayout;
	std::vector<ResourceManager::ResourceCopyInfo> copyInfo;
};
MergeInfo getMergeInfo(
	std::span<const BufferHandle> previousBuffers,
	std::span<const BufferHandle> newBuffers,
	const std::vector<SceneLoader::SceneResources>& sceneData,
	std::size_t skipIndex
) {
	// Merge previous buffers to new buffers
	MergeInfo mergeInfo;
	for (int i = 0; i < skipIndex; i++) {
		for (int b = 0; b < 6; b++) {
			mergeInfo.buffersLayout[b].size += sceneData[i].bufferDataLocations[b].size;
			if (b > 1) mergeInfo.buffersLayout[b].offset += sceneData[i].bufferDataLocations[b - 1].size;
		}
	}
	mergeInfo.copyInfo.resize(6);
	for (int b = 0; b < 6; b++) {
		mergeInfo.copyInfo[b] = {
			.source =
				ResourceManager::ResourceCopyInfo::BufferReference {
																	.handle = previousBuffers[b],
																	.size = (uint32_t)mergeInfo.buffersLayout[b].size,
																	.offset = (uint32_t)mergeInfo.buffersLayout[b].offset,
																	},

			.destination =
				ResourceManager::ResourceCopyInfo::BufferReference {
																	.handle = newBuffers[b],
																	.size = (uint32_t)mergeInfo.buffersLayout[b].size,
																	.offset = (uint32_t)mergeInfo.buffersLayout[b].offset,
																	}
		};
	}

	if (skipIndex < sceneData.size() - 1) {
		for (int b = 0; b < 6; b++) mergeInfo.buffersLayout[b].size = 0;

		for (int i = skipIndex + 1; i < sceneData.size(); i++) {
			for (int b = 0; b < 6; b++) {
				mergeInfo.buffersLayout[b].size += sceneData[i].bufferDataLocations[b].size;
				if (b > 1) mergeInfo.buffersLayout[b].offset += sceneData[i].bufferDataLocations[b - 1].size;
			}
		}

		for (int b = 0; b < 6; b++) {
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
	const std::array<MemorySpan, 6> stagingLayout,
	const std::array<MemorySpan, 6> currentLayout,
	std::span<const BufferHandle> bufferHandles
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
																		.offset = static_cast<uint32_t>(stagingLayout[i].offset),
																		},
				.destination = ResourceManager::ResourceCopyInfo::BufferReference {
																		.handle = bufferHandles[i],
																		.size = static_cast<uint32_t>(stagingLayout[i].size),
																		.offset = static_cast<uint32_t>(currentLayout[i].offset + stagingLayout[i].offset),

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

void SceneManager::loadAsync(const std::filesystem::path& scene) {
	if (!std::filesystem::exists(scene)) {
		std::cout << "Scene not found : " + scene.relative_path().string() << std::endl;
		abort();
	}
	m_loadingData.emplace(scene);

	m_sceneData.push_back(m_loadingData->sceneLoader.querySceneResources());
	auto& resources = m_sceneData.back();

	auto stagingAllocation = m_resourceManager.createResources(
		{
	},
		{ {
			.size = static_cast<uint32_t>(resources.buffersStagingSize + resources.imageStagingSize),
			.usage = vk::BufferUsageFlagBits::eTransferSrc,
		} },
		ResourceManager::MemoryLocation::Host
	);

	auto geometryBuffersDescription = getGeometryAllocationData(m_sceneData);
	auto geometryAllocation = m_resourceManager.createResources(
		{}, geometryBuffersDescription.buffers, ResourceManager::MemoryLocation::Device
	);
	auto mergeInfo = getMergeInfo(
		m_resourceManager.getBuffers(m_geometryAllocation),
		m_resourceManager.getBuffers(geometryAllocation),
		m_sceneData,
		m_sceneData.size() - 1
	);

	m_resourceManager.copyResources(mergeInfo.copyInfo);

	auto imageDescriptions = getImageDescriptions(resources.imageResolution, resources.imageFormats);
	auto textureAllocation =
		m_resourceManager.createResources(imageDescriptions, {}, ResourceManager::MemoryLocation::Device);

	Buffer& stagingBuffer = m_resourceManager.getBuffer(m_resourceManager.getBuffers(stagingAllocation)[0]);

	m_loadingData->sceneLoader.beginImageLoad(stagingBuffer.data);
	m_loadingData->sceneLoader.beginBufferLoad(stagingBuffer.data);

	m_sceneTextureAllocations.push_back(textureAllocation);
}

SceneManager::LoadedPercentage SceneManager::sync() {
	auto delta = m_loadingData->sceneLoader.queryLoadStatus();
	std::size_t sceneIndex = m_sceneData.size() - 1;
	auto& currentSceneData = m_sceneData[sceneIndex];

	SceneManager::LoadedPercentage percentage = 0;

	std::size_t totalResourceCount =
		(currentSceneData.imageDataLocations.size() + currentSceneData.bufferDataLocations.size());

	percentage += (float)(m_loadingData->resourceLoadedCount + delta.loadedImages.size() + delta.loadedBuffers.size()) /
				  totalResourceCount;

	m_loadingData->resourceLoadedCount += delta.loadedImages.size();
	m_loadingData->resourceLoadedCount += delta.loadedBuffers.size();

	if (delta.loadedImages.size() > 0) {
		auto imageUploadInfo = getImagesUploadInfo(
			m_loadingData->stagingBuffer,
			delta.loadedImages,
			currentSceneData.imageDataLocations,
			currentSceneData.imageResolution,
			m_resourceManager.getImages(m_sceneTextureAllocations[sceneIndex])
		);
		m_resourceManager.copyResources(imageUploadInfo);
	}
	if (delta.loadedBuffers.size() > 0) {
		auto bufferUploadInfo = getBuffersUploadInfo(
			m_loadingData->stagingBuffer,
			currentSceneData.bufferDataLocations,
			m_buffersLayout,
			m_resourceManager.getBuffers(m_sceneTextureAllocations[sceneIndex])
		);
	}

	return percentage;
}

struct Offsets {
	uint32_t vertexOffset = 0;
	uint32_t indexOffset = 0;
};

void loadPrimitives(
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

Scene SceneManager::getScene() {
	if (m_loadingData) {
		m_scenes.push_back(std::move(m_loadingData->sceneLoader).getScene());
		m_loadingData = std::nullopt;
	}
	Scene res;

	Offsets offsets;
	for (int i = 0; i < m_scenes.size(); i++) {
		auto& scene = m_scenes[i];

		res.materialHint.insert(res.materialHint.end(), scene.materialHint.begin(), scene.materialHint.end());
		res.primitiveBounds.insert(
			res.primitiveBounds.end(), scene.primitiveBounds.begin(), scene.primitiveBounds.end()
		);

		res.size = std::max(res.size, scene.size);
		loadPrimitives(scene.primitives, res.primitives, offsets);

		auto& sceneData = m_sceneData[i];
		std::size_t vertexCount = sceneData.bufferDataLocations[GeometryBufferType::Vertex].size / sizeof(Vertex);
		std::size_t indexCount = sceneData.bufferDataLocations[GeometryBufferType::Index].size / sizeof(uint32_t);

		offsets.vertexOffset += vertexCount;
		offsets.indexOffset += indexCount;
	}

	return res;
}
