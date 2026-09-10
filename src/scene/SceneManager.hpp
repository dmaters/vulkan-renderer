#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <vector>

#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneLoader.hpp"

class SceneManager {
public:
	ResourceManager& m_resourceManager;

	enum GeometryBufferType {
		Vertex,
		VertexAttribute,
		Index,
		Transforms,
		MaterialInstances,
		Materials,
	};

private:
	struct SceneResources {
		std::array<std::size_t, 6> bufferSizes;

		std::vector<uint8_t> imageMipCount;
		std::vector<std::size_t> imageBaseSize;

		std::size_t occupiedStagingOffset;
	};
	struct LoadingData {
		SceneLoader sceneLoader;
		BufferHandle stagingBuffer;
		ResourceManager::AllocationIndex newAllocation;
		std::size_t resourceLoadedCount = 0;
	};
	std::optional<LoadingData> m_loadingData;

	std::array<MemorySpan, 6> m_buffersLayout;
	std::vector<SceneLoader::SceneInstance> m_scenes;
	std::vector<SceneLoader::SceneResources> m_sceneData;
	std::vector<ResourceManager::AllocationIndex> m_sceneTextureAllocations;

	// TODO: dispose safely of previous allocation
	ResourceManager::AllocationIndex m_geometryAllocation;
	ResourceManager::AllocationIndex m_dummyAllocation;

public:
	SceneManager(ResourceManager&);

	using ResourceCount = std::size_t;
	ResourceCount loadAsync(const std::filesystem::path&);

	using LoadedResourceCount = std::size_t;
	LoadedResourceCount sync();

	Scene getScene();

	ResourceManager::AllocationIndex getBuffersAllocation() { return m_geometryAllocation; }
};
