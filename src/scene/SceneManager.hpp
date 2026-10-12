#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <vector>

#include "material/MaterialManager.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneLoader.hpp"

class SceneManager {
public:
	template <typename T>
	union SharedBufferContainer;

private:
	struct LoadingData {
		std::unique_ptr<SceneLoader> sceneLoader;
		Scene mergedScene;
		ResourceManager::AllocationIndex stagingAllocation;
		ResourceManager::AllocationIndex sharedAllocation;
		std::size_t resourceLoadedCount = 0;

		vk::CommandPool commandPool;
	};

	vk::Semaphore m_semaphore;
	uint64_t m_transferCount = 0;

	ResourceManager& m_resourceManager;
	MaterialManager& m_materialManager;

	std::optional<LoadingData> m_loadingData;


	std::vector<SceneLoader::SceneInstance> m_scenes;
	std::vector<SceneLoader::SceneGeometry> m_scenesGeometry;
	std::vector<ResourceManager::AllocationIndex> m_sceneTextureAllocations;
	using SceneGeometryBufferDescriptions =
		std::array<ResourceManager::BufferDescription, SceneLoader::SceneBuffersCount>;
	std::vector<SceneGeometryBufferDescriptions> m_sharedBuffersDescriptions;

	ResourceManager::AllocationIndex m_dummyAllocation;
	std::optional<ResourceManager::AllocationIndex> m_sharedAllocation;

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

template <typename T>
union SceneManager::SharedBufferContainer {
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
