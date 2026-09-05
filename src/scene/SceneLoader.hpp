#pragma once

#include <fastgltf/core.hpp>
#include <filesystem>
#include <texture_compressor/compression.hpp>

#include "Scene.hpp"
#include "utils/ConcurrentStack.hpp"

class SceneLoader {
public:
	struct SceneResources {
		struct MemorySpan {
			std::size_t size = 0;
			std::size_t offset = 0;
		};
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

	std::array<SceneResources::MemorySpan, 6> m_bufferDataLocations;
	bool m_buffersLoaded = false;
	bool m_imagesLoaded = false;

	std::vector<SceneResources::MemorySpan> m_imageDataLocations;
	std::shared_ptr<ConcurrentStack<std::size_t>> m_readyImages;

	using TextureUsage = uint8_t;
	std::vector<TextureUsage> m_textureUsages;

public:
	SceneLoader(const std::filesystem::path& path) : m_path(path) {}
	SceneResources querySceneResources();

	void beginBufferLoad(void* stagingAddress);

	void beginImageLoad(void* stagingAddress);

	struct LoadStatus {
		std::vector<uint32_t> imageLoadedDelta;
		bool buffersLoaded;
	};
	LoadStatus queryLoadStatus();

	Scene getScene() &&;
};
