#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <vector>

#include "AccelerationStructureBuilder.hpp"
#include "material/MaterialManager.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneLoader.hpp"

class SceneManager {
public:
	enum class SceneBufferType {
		Vertex,
		VertexAttribute,
		Index,
		MaterialData,
		Transforms,
		PrimitiveData,
	};

private:
	struct LoadingData {
		SceneLoader sceneLoader;
		Scene scene;
		ResourceManager::AllocationIndex stagingAllocation;
		ResourceManager::AllocationIndex newAllocation;
		std::size_t resourceLoadedCount = 0;

		vk::CommandPool commandPool;
	};

	vk::Semaphore m_semaphore;
	uint64_t m_transferCount = 0;

	ResourceManager& m_resourceManager;
	MaterialManager& m_materialManager;

	std::optional<LoadingData> m_loadingData;

	std::size_t m_primitiveCount;

	std::array<MemorySpan, SceneLoader::SceneBuffersCount> m_buffersLayout;
	std::vector<SceneLoader::SceneInstance> m_scenes;
	std::vector<SceneLoader::SceneGeometry> m_scenesGeometry;
	std::vector<ResourceManager::AllocationIndex> m_sceneTextureAllocations;

	ResourceManager::AllocationIndex m_dummyAllocation;
	std::optional<ResourceManager::AllocationIndex> m_sceneAllocation;

	Scene m_scene;

public:
	SceneManager(ResourceManager&, MaterialManager&);

	using ResourceCount = std::size_t;
	ResourceCount loadAsync(const std::filesystem::path&);

	using LoadedResourceCount = std::size_t;
	LoadedResourceCount sync();

	struct SyncronizationData {
		vk::Semaphore semaphore;
		uint64_t value;
	};

	Scene getScene();
};
