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
	using SceneIndex = uint32_t;
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
	std::vector<Scene> m_scenes;
	std::vector<SceneLoader::SceneResources> m_sceneData;
	std::vector<ResourceManager::AllocationIndex> m_sceneTextureAllocations;

	// TODO: dispose safely of previous allocation
	ResourceManager::AllocationIndex m_geometryAllocation;

public:
	SceneManager(ResourceManager& resourceManager) : m_resourceManager(resourceManager) {}

	void loadAsync(const std::filesystem::path& scene);

	using LoadedPercentage = float;
	LoadedPercentage sync();

	Scene getScene();
};
