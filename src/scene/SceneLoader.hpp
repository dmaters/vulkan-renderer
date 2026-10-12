#pragma once

#include <fastgltf/core.hpp>
#include <filesystem>
#include <texture_compressor/compression.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "Common.hpp"
#include "Scene.hpp"
#include "material/MaterialDefinitions.hpp"
#include "resources/ResourceManager.hpp"
#include "utils/ConcurrentStack.hpp"

class SceneLoader {
public:
	static const uint32_t SceneBuffersCount = 5;

	struct SceneInstance {
		std::vector<Primitive> primitives;
		std::vector<Scene::MaterialHint> materialHints;
		std::vector<glm::mat4> transforms;

		std::size_t vertexCount;
		std::size_t indexCount;
		std::size_t materialCount;
	};

	struct SceneAllocationInfo {
		std::array<ResourceManager::BufferDescription, SceneBuffersCount> geometryBuffers;
		std::vector<ResourceManager::BufferDescription> stagingSharedBuffers;

		std::vector<ResourceManager::ImageDescription> images;
		std::vector<ResourceManager::BufferDescription> stagingImageBuffers;
	};

	struct SceneGeometry {
		std::vector<Scene::PrimitiveBound> primitiveBounds;
		float size = 0.0;
	};

private:
	std::filesystem::path m_path;
	fastgltf::Asset m_asset;
	SceneInstance m_scene;
	SceneGeometry m_sceneGeometry;
	std::array<std::size_t, SceneLoader::SceneBuffersCount> m_bufferSizes;

	ConcurrentStack<std::size_t> m_readyImages;

	// Readybuffers is a faux stack that uses the same logic path of images, but we have for now only one thread
	// handling buffer loading, so the buffer will always be loaded/signaled in bulk
	ConcurrentStack<std::size_t> m_readyBuffers;

	using TextureUsage = uint8_t;
	std::vector<TextureUsage> m_textureUsages;

public:
	SceneLoader(std::filesystem::path path);

	SceneInstance getSceneInstance() { return m_scene; }
	SceneAllocationInfo getAllocationInfo();

	void beginBufferLoad(std::span<void*> addresses, std::vector<std::size_t> registeredImageIndices);
	void beginImageLoad(std::span<void*> stagingAddress);

	struct LoadStatus {
		std::vector<std::size_t> loadedImages;
		std::vector<std::size_t> loadedBuffers;
	};
	LoadStatus queryLoadStatus();

	SceneGeometry getSceneGeometry();
};
