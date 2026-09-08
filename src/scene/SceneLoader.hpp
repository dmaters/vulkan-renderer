#pragma once

#include <fastgltf/core.hpp>
#include <filesystem>
#include <texture_compressor/compression.hpp>

#include "Common.hpp"
#include "Scene.hpp"
#include "utils/ConcurrentStack.hpp"

class SceneLoader {
public:
	struct SceneResources {
		std::array<MemorySpan, 6> bufferDataLocations;

		std::vector<MemorySpan> imageDataLocations;
		std::vector<vk::Format> imageFormats;
		std::vector<glm::ivec2> imageResolution;

		std::size_t buffersStagingSize;
		std::size_t imageStagingSize;
	};

private:
	const std::filesystem::path& m_path;
	fastgltf::Asset m_asset;
	Scene m_scene;

	std::array<MemorySpan, 6> m_bufferDataLocations;

	std::vector<MemorySpan> m_imageDataLocations;
	ConcurrentStack<std::size_t> m_readyImages;

	// Readybuffers is a faux stack that uses the same logic path of images, but we have for now only one thread
	// handling buffer loading, so the buffer will always be loaded/signaled in bulk
	ConcurrentStack<std::size_t> m_readyBuffers;

	using TextureUsage = uint8_t;
	std::vector<TextureUsage> m_textureUsages;

public:
	SceneLoader(const std::filesystem::path& path) : m_path(path) {}
	SceneResources querySceneResources();

	void beginBufferLoad(void* stagingAddress);

	void beginImageLoad(void* stagingAddress);

	struct LoadStatus {
		std::vector<std::size_t> loadedImages;
		std::vector<std::size_t> loadedBuffers;
	};
	LoadStatus queryLoadStatus();

	Scene getScene() &&;
};
