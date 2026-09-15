#pragma once

#include <fastgltf/core.hpp>
#include <filesystem>
#include <texture_compressor/compression.hpp>
#include <vulkan/vulkan.hpp>

#include "Common.hpp"
#include "Scene.hpp"
#include "material/MaterialDefinitions.hpp"
#include "utils/ConcurrentStack.hpp"

class SceneLoader {
public:
	static const uint32_t SceneBuffersCount = 5;

	struct SceneInstance {
		std::vector<Primitive> primitives;
		std::vector<Scene::MaterialHint> materialHints;

		std::array<MemorySpan, SceneBuffersCount> bufferDataLocations;

		std::vector<MemorySpan> imageDataLocations;
		std::vector<vk::Format> imageFormats;
		std::vector<glm::ivec2> imageResolution;

		std::size_t buffersStagingSize;
		std::size_t imageStagingSize;
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

	std::array<MemorySpan, SceneBuffersCount> m_bufferDataLocations;
	std::vector<MemorySpan> m_imageDataLocations;
	ConcurrentStack<std::size_t> m_readyImages;

	// Readybuffers is a faux stack that uses the same logic path of images, but we have for now only one thread
	// handling buffer loading, so the buffer will always be loaded/signaled in bulk
	ConcurrentStack<std::size_t> m_readyBuffers;

	using TextureUsage = uint8_t;
	std::vector<TextureUsage> m_textureUsages;

public:
	SceneLoader(std::filesystem::path path) : m_path(path) {}
	SceneInstance getInstance();

	void beginBufferLoad(void* stagingAddress, std::vector<std::size_t> registeredImageIndices);

	void beginImageLoad(void* stagingAddress);

	struct LoadStatus {
		std::vector<std::size_t> loadedImages;
		std::vector<std::size_t> loadedBuffers;
	};
	LoadStatus queryLoadStatus();

	SceneGeometry getSceneGeometry();
};
