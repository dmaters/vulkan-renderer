#include "scene/SceneManager.hpp"

#include "Instance.hpp"

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

std::vector<ResourceManager::ResourceCopyInfo> getMergeInfo(
	std::span<const BufferHandle> previousBuffers,
	std::span<const BufferHandle> newBuffers,
	const std::vector<SceneLoader::SceneResources>& sceneData,
	std::size_t skipIndex
) {
	// Merge previous buffers to new buffers

	std::array<SceneLoader::SceneResources::MemorySpan, 6> bufferDataLocations;
	for (int i = 0; i < sceneData.size(); i++) {
	    bufferDataLocations{}
	
	}

	for (int i = 0; i < 6; i++) {
		copyInfo.push_back(
			{
				.source =
					ResourceManager::ResourceCopyInfo::BufferReference {
																		.handle = previousBuffers[i],
																		.size = 0,
																		.offset = 0,
																		},
				.source = ResourceManager::ResourceCopyInfo::BufferReference {
																		.handle = newBuffers[i],
																		.size = 0,
																		.offset = 0,
																		}
		  }
		)
	}
	for (int i = 0; i < sceneData.size(); i++) {
	}
}

std::vector<ResourceManager::ResourceCopyInfo> getBuffersUploadInfo(
	BufferHandle stagingBuffer,
	const std::array<SceneLoader::SceneResources::MemorySpan, 6> newSceneBuffers,
	const std::array<std::size_t, 6> mainBuffersOffsets,
	std::span<const BufferHandle> bufferHandles
) {
	std::vector<ResourceManager::ResourceCopyInfo> copies;
	copies.reserve(newSceneBuffers.size());

	for (int i = 0; i < newSceneBuffers.size(); i++) {
		copies.push_back(
			{
				.source =
					ResourceManager::ResourceCopyInfo::BufferReference {
																		.handle = stagingBuffer,
																		.size = static_cast<uint32_t>(newSceneBuffers[i].size),
																		.offset = static_cast<uint32_t>(newSceneBuffers[i].offset),
																		},
				.destination = ResourceManager::ResourceCopyInfo::BufferReference {
																		.handle = bufferHandles[i],
																		.size = static_cast<uint32_t>(newSceneBuffers[i].size),
																		.offset = static_cast<uint32_t>(mainBuffersOffsets[i]),

																		}
		  }
		);
	}
	return copies;
}

std::vector<ResourceManager::ResourceCopyInfo> getImagesUploadInfo(
	BufferHandle stagingBuffer,
	const std::vector<SceneLoader::SceneResources::MemorySpan>& images,
	const std::vector<uint8_t>& mips,
	std::span<const ImageHandle> handles
) {
	std::vector<ResourceManager::ResourceCopyInfo> copies;

	for (int i = 0; i < images.size(); i++) {
		std::size_t mipOffset = 0;

		for (int mip = 0; mip < mips[i]; mip++) {
			std::size_t mipSize = images[i].size >> (mip * 2);
			copies.push_back(
				{
					.source =
						ResourceManager::ResourceCopyInfo::BufferReference {
																			.handle = stagingBuffer,
																			.size = static_cast<uint32_t>(mipSize),
																			.offset = static_cast<uint32_t>(images[i].offset + mipOffset),
																			},
					.destination = ResourceManager::ResourceCopyInfo::ImageReference {
																			.handle = handles[i],
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

std::optional<SceneManager::SceneIndex> SceneManager::loadAsync(const std::filesystem::path& scene) {
	if (!std::filesystem::exists(scene)) return std::nullopt;

	SceneLoader loader(scene);

	m_sceneData.push_back(loader.querySceneResources());
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

	m_resourceManager.copyResources(const std::vector<ResourceCopyInfo>& info) auto imageDescriptions =
		getImageDescriptions(resources.imageResolution, resources.imageFormats);
	auto textureAllocation =
		m_resourceManager.createResources(imageDescriptions, {}, ResourceManager::MemoryLocation::Device);

	Buffer& stagingBuffer = m_resourceManager.getBuffer(m_resourceManager.getBuffers(stagingAllocation)[0]);

	loader.beginImageLoad(stagingBuffer.data);
	loader.beginBufferLoad(stagingBuffer.data);

	m_sceneTextureAllocations.push_back(textureAllocation);
}

void SceneManager::uploadResourceBatch(SceneIndex index) { auto delta = m_loaders[index].queryLoadStatus(); }
