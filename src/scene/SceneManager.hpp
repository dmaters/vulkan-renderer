#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include "resources/ResourceManager.hpp"
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

	std::vector<SceneLoader::SceneResources> m_sceneData;
	std::vector<SceneLoader> m_loaders;
	std::vector<ResourceManager::AllocationIndex> m_sceneTextureAllocations;

	// TODO: dispose safely of previous allocation
	ResourceManager::AllocationIndex m_geometryAllocation;

public:
	SceneManager(ResourceManager& resourceManager) : m_resourceManager(resourceManager) {}

	std::optional<SceneIndex> loadAsync(const std::filesystem::path& scene);
	void uploadResourceBatch(SceneIndex index);
	void getLoadedPercentage(SceneIndex index);
};
