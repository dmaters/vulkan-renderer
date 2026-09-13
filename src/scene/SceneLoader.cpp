#include "SceneLoader.hpp"

#include <memory>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <fastgltf/core.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <texture_compressor/common.hpp>
#include <texture_compressor/compression.hpp>
#include <texture_compressor/utils.hpp>
#include <thread>
#include <vector>

#include "material/MaterialDefinitions.hpp"
#include "utils/ConcurrentStack.hpp"
enum TextureUsage : uint8_t {
	Albedo,
	AlbedoWithAlpha,
	Normal,
	RoughnessMetallic,
};

using TextureUsageType = uint8_t;

struct VertexAttributes {
	glm::vec3 normal;
	glm::vec3 tangent;
	glm::vec3 bitangent;
	glm::vec2 texcoord;
};

enum class SceneBuffers {
	Vertex,
	VertexAttribute,
	Indices,
	Transforms,
	Materials,
	PrimitiveData,
};

inline texture_compressor::Format getFormatFromUsage(TextureUsageType usage) {
	switch (usage) {
		case TextureUsage::Albedo:
			return texture_compressor::Format::BC1;
		case TextureUsage::AlbedoWithAlpha:
			return texture_compressor::Format::BC1_ALPHA;
		case TextureUsage::Normal:
		case TextureUsage::RoughnessMetallic:
			return texture_compressor::Format::BC5;
	}
	return texture_compressor::Format::BC1;
}

vk::Format getFormat(texture_compressor::Format format) {
	switch (format) {
		case texture_compressor::Format::BC1:
		case texture_compressor::Format::BC1_ALPHA:
			return vk::Format::eBc1RgbaSrgbBlock;
		case texture_compressor::Format::BC5:
			return vk::Format::eBc5UnormBlock;
		default:
			return vk::Format::eUndefined;
	}
	return vk::Format::eUndefined;
}

std::vector<TextureUsageType> getTextureUsages(const fastgltf::Asset& asset) {
	std::vector<TextureUsageType> formats(asset.images.size());

	for (auto& material : asset.materials) {
		if (material.pbrData.baseColorTexture.has_value()) {
			std::size_t baseColor = asset.textures[material.pbrData.baseColorTexture->textureIndex].imageIndex.value();

			if (material.alphaMode == fastgltf::AlphaMode::Mask)
				formats[baseColor] = TextureUsage::AlbedoWithAlpha;
			else
				formats[baseColor] = TextureUsage::Albedo;
		}

		if (material.normalTexture.has_value()) {
			std::size_t normal = asset.textures[material.normalTexture->textureIndex].imageIndex.value();

			formats[normal] = TextureUsage::Normal;
		}

		if (material.pbrData.metallicRoughnessTexture.has_value()) {
			std::size_t roughnessMetallic =
				asset.textures[material.pbrData.metallicRoughnessTexture->textureIndex].imageIndex.value();

			formats[roughnessMetallic] = TextureUsage::RoughnessMetallic;
		}
	}

	return formats;
}

struct PrimitiveData {
	std::vector<Primitive> primitives;
	std::vector<Scene::MaterialHint> materialHints;
	std::array<MemorySpan, SceneLoader::SceneBuffersCount> bufferDataLocations;

	std::size_t bufferStagingSize = 0;
};

PrimitiveData loadPrimitiveData(const fastgltf::Asset& asset) {
	std::array<MemorySpan, SceneLoader::SceneBuffersCount> bufferDataLocations;

	std::vector<Primitive> primitives;
	std::vector<Scene::MaterialHint> materialHints;

	std::size_t vertexOffset = 0, indexOffset = 0;
	for (auto& mesh : asset.nodes) {
		if (!mesh.meshIndex.has_value()) continue;

		for (auto& primitive : asset.meshes[mesh.meshIndex.value()].primitives) {
			std::size_t vertexCount = asset.accessors[primitive.findAttribute("POSITION")->accessorIndex].count;

			bufferDataLocations[(int)SceneBuffers::Vertex].size += vertexCount * sizeof(glm::vec3);
			bufferDataLocations[(int)SceneBuffers::VertexAttribute].size += vertexCount * sizeof(VertexAttributes);
			std::size_t indexCount = 0;
			if (primitive.indicesAccessor.has_value()) {
				indexCount = asset.accessors[primitive.indicesAccessor.value()].count;
				bufferDataLocations[(int)SceneBuffers::Indices].size += indexCount * sizeof(uint32_t);
			} else {
				indexCount = vertexCount;
				bufferDataLocations[(int)SceneBuffers::Indices].size += indexCount * sizeof(uint32_t);
			}

			bufferDataLocations[(int)SceneBuffers::Transforms].size += sizeof(glm::mat4);
			primitives.push_back(
				{
					.baseVertex = (uint32_t)vertexOffset,
					.baseIndex = (uint32_t)indexOffset,
					.indexCount = (uint32_t)indexCount,
					.materialIndex = (uint32_t)primitive.materialIndex.value_or(0),
				}
			);

			if (asset.materials[primitive.materialIndex.value()].alphaMode == fastgltf::AlphaMode::Mask)
				materialHints.push_back(
					Scene::MaterialHintBits::Opaque | Scene::MaterialHintBits::ShadowCasting |
					Scene::MaterialHintBits::AlphaMask
				);
			else
				materialHints.push_back(Scene::MaterialHintBits::Opaque | Scene::MaterialHintBits::ShadowCasting);

			vertexOffset += vertexCount;
			indexOffset += indexCount;
		}
	}

	bufferDataLocations[(int)SceneBuffers::Materials].size =
		asset.materials.size() * sizeof(MaterialDefinitions::PBRInstance);

	std::size_t bufferStagingSize = 0;
	for (int i = 0; i < bufferDataLocations.size(); i++) {
		bufferStagingSize += bufferDataLocations[i].size;
		if (i > 0) bufferDataLocations[i].offset = bufferDataLocations[i - 1].offset + bufferDataLocations[i - 1].size;
	}

	return {
		.primitives = primitives,
		.materialHints = materialHints,
		.bufferDataLocations = bufferDataLocations,
		.bufferStagingSize = bufferStagingSize,
	};
}

static uint8_t getMipLevels(int width, int height) {
	uint32_t mipLevels = std::floor(std::log2(std::min(width, height))) + 1;
	mipLevels = mipLevels <= 3 ? 1 : mipLevels - 3;
	return mipLevels;
}

struct ImageData {
	std::vector<MemorySpan> imageDataLocations;
	std::vector<vk::Format> imageFormats;
	std::vector<glm::ivec2> imageResolution;

	std::size_t imageStagingSize = 0;
};

ImageData loadImageData(
	const fastgltf::Asset& asset,
	const std::filesystem::path& folderPath,
	const std::vector<TextureUsageType>& textureUsages
) {
	std::size_t offset = 0;
	std::vector<MemorySpan> imageDataLocations;
	std::vector<vk::Format> formats;
	std::vector<glm::ivec2> resolutions;

	imageDataLocations.reserve(asset.images.size());
	formats.reserve(asset.images.size());
	resolutions.reserve(asset.images.size());

	for (int i = 0; i < asset.images.size(); i++) {
		auto& image = asset.images[i];
		int width, height, channels;

		std::visit(
			fastgltf::visitor {
				[&](const fastgltf::sources::URI& uri) {
					stbi_info((folderPath / uri.uri.path()).string().c_str(), &width, &height, &channels);
				},
				[&](const fastgltf::sources::Array& vector) {
					stbi_info_from_memory(
						(stbi_uc*)vector.bytes.data(), vector.bytes.size_bytes(), &width, &height, &channels
					);
				},
				[&](const fastgltf::sources::BufferView& view) {
					auto& bufferView = asset.bufferViews[view.bufferViewIndex];
					auto& buffer = asset.buffers[bufferView.bufferIndex];
					std::visit(
						fastgltf::visitor {
							[](auto& arg) {},
							[&](fastgltf::sources::Array& vector) {
								stbi_info_from_memory(
									(stbi_uc*)vector.bytes.data(), vector.bytes.size_bytes(), &width, &height, &channels
								);
							} },
						buffer.data
					);
				},
				[](auto& arg) {},
			},
			image.data
		);

		uint8_t mipLevels = getMipLevels(width, height);
		auto compressedFormat = getFormatFromUsage(textureUsages[i]);
		std::size_t imageSize = texture_compressor::query_size(width, height, compressedFormat);
		imageSize = std::max(imageSize, std::size_t(16));

		imageDataLocations.push_back(
			{
				.size = imageSize,
				.offset = offset,
			}
		);

		formats.push_back(getFormat(compressedFormat));
		resolutions.push_back({ width, height });

		for (int i = 0; i < mipLevels; i++) {
			offset += imageSize >> (i * 2);
		}
	}
	return {
		.imageDataLocations = imageDataLocations,
		.imageFormats = formats,
		.imageResolution = resolutions,
		.imageStagingSize = offset,
	};
}

SceneLoader::SceneInstance loadSceneInstance(
	const fastgltf::Asset& asset,
	const std::filesystem::path& folderPath,
	const std::vector<TextureUsageType>& textureUsages
) {
	auto primitiveData = loadPrimitiveData(asset);

	auto imageData = loadImageData(asset, folderPath, textureUsages);

	return {
		.primitives = primitiveData.primitives,
		.materialHints = primitiveData.materialHints,
		.bufferDataLocations = primitiveData.bufferDataLocations,
		.imageDataLocations = imageData.imageDataLocations,
		.imageFormats = imageData.imageFormats,
		.imageResolution = imageData.imageResolution,
		.buffersStagingSize = primitiveData.bufferStagingSize,
		.imageStagingSize = imageData.imageStagingSize,
	};
}

SceneLoader::SceneInstance SceneLoader::getInstance() {
	fastgltf::Parser parser;
	auto data = fastgltf::GltfDataBuffer::FromPath(m_path);
	m_asset = *parser.loadGltf(data.get(), m_path.parent_path(), fastgltf::Options::LoadExternalBuffers);
	m_textureUsages = getTextureUsages(m_asset);
	auto sceneInstance = loadSceneInstance(m_asset, m_path.parent_path(), m_textureUsages);

	m_bufferDataLocations = sceneInstance.bufferDataLocations;
	m_imageDataLocations = sceneInstance.imageDataLocations;

	return sceneInstance;
}

struct PrimitiveGeometryData {
	Scene::PrimitiveBound primitiveBounds;
	std::size_t vertexCount;
	std::size_t indexCount;
};

PrimitiveGeometryData loadPrimitiveGeometry(
	const fastgltf::Asset& asset,
	const fastgltf::Primitive& primitive,
	glm::vec3* vertices,
	VertexAttributes* vertexAttributes,
	uint32_t* indices
) {
	auto& vertexAccessor = asset.accessors[primitive.findAttribute("POSITION")->accessorIndex];
	float size2 = 0;
	glm::vec3 averageVertexPosition = glm::vec3(0);

	fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, vertexAccessor, [&](glm::vec3 vertex, std::size_t index) {
		vertices[index] = vertex;
		size2 = std::max(size2, glm::dot(vertex, vertex));
		averageVertexPosition += vertex;
	});
	averageVertexPosition /= vertexAccessor.count;

	std::size_t indexCount = 0;
	if (primitive.indicesAccessor.has_value()) {
		auto& indexAccessor = asset.accessors[primitive.indicesAccessor.value()];

		fastgltf::iterateAccessorWithIndex<uint32_t>(
			asset, indexAccessor, [&](uint32_t vertexIndex, std::size_t index) { indices[index] = vertexIndex; }
		);
		indexCount = indexAccessor.count;
	} else {
		for (int i = 0; i < vertexAccessor.count; i++) {
			indices[i] = i;
		}
		indexCount = vertexAccessor.count;
	}

	if (primitive.findAttribute("NORMAL") != primitive.attributes.end()) {
		auto& normalAccessor = asset.accessors[primitive.findAttribute("NORMAL")->accessorIndex];

		fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, normalAccessor, [&](glm::vec3 normal, std::size_t index) {
			vertexAttributes[index].normal = normal;
		});
	} else {
		for (int i = 0; i < indexCount; i += 3) {
			uint32_t i1 = indices[i + 0];
			uint32_t i2 = indices[i + 1];
			uint32_t i3 = indices[i + 2];

			glm::vec3 v1 = vertices[i1];
			glm::vec3 v2 = vertices[i2];
			glm::vec3 v3 = vertices[i3];

			vertexAttributes[i1].normal = glm::normalize(glm::cross(v2 - v1, v3 - v1));
			vertexAttributes[i2].normal = glm::normalize(glm::cross(v2 - v1, v3 - v1));
			vertexAttributes[i3].normal = glm::normalize(glm::cross(v2 - v1, v3 - v1));
		}
	}

	for (int i = 0; i < vertexAccessor.count; i++) {
		// Pixar - Building an Orthonormal Basis, Revisited
		// (2017)

		glm::vec3 normal = vertexAttributes[i].normal;

		float sign = copysign(1.0f, normal.z);
		float a = -1.0f / (sign + normal.z);
		float b = normal.x * normal.y * a;
		vertexAttributes[i].tangent = glm::vec3(1.0f + sign * normal.x * normal.x * a, sign * b, -sign * normal.x);
		vertexAttributes[i].bitangent = glm::vec3(b, sign + normal.y * normal.y * a, -normal.y);
	}

	auto& texcoordAccessor = asset.accessors[primitive.findAttribute("TEXCOORD_0")->accessorIndex];
	fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, texcoordAccessor, [&](glm::vec2 texcoord, std::size_t index) {
		vertexAttributes[index].texcoord = texcoord;
	});

	return {
	    .primitiveBounds = {
			.position = averageVertexPosition,
			.size = sqrt(size2) + glm::length(averageVertexPosition),
		},
		.vertexCount = vertexAccessor.count,
		.indexCount = indexCount ,
	};
}

struct SceneGeometry {
	std::vector<Scene::PrimitiveBound> primitivesBounds;
	float sceneSize;
};

SceneGeometry loadGeometryBuffers(
	const fastgltf::Asset& asset,
	glm::vec3* vertices,
	VertexAttributes* vertexAttributes,
	uint32_t* indices,
	glm::mat4* transforms
) {
	std::vector<Scene::PrimitiveBound> primitiveBounds;
	primitiveBounds.reserve(asset.meshes.size());

	float sceneSize = 0;
	fastgltf::iterateSceneNodes(
		asset, 0, fastgltf::math::fmat4x4(), [&](const fastgltf::Node& node, const fastgltf::math::fmat4x4& matrix) {
			if (!node.meshIndex.has_value()) return;

			const fastgltf::Mesh& mesh = asset.meshes[node.meshIndex.value()];
			for (auto& primitive : mesh.primitives) {
				auto primitiveData = loadPrimitiveGeometry(asset, primitive, vertices, vertexAttributes, indices);
				vertices += primitiveData.vertexCount;
				vertexAttributes += primitiveData.vertexCount;
				indices += primitiveData.indexCount;

				*transforms = (glm::mat4&)matrix;
				transforms += 1;

				primitiveBounds.push_back(primitiveData.primitiveBounds);
				sceneSize = std::max(primitiveData.primitiveBounds.size, sceneSize);
			}
		}
	);
	return {
		.primitivesBounds = primitiveBounds,
		.sceneSize = sceneSize,
	};
}

void loadMaterials(
	const fastgltf::Asset& asset,
	MaterialDefinitions::PBRInstance* materials,
	const std::vector<std::size_t>& registeredImageIndices
) {
	for (int i = 0; i < asset.materials.size(); i++) {
		auto& material = asset.materials[i];
		materials[i] = {};

		if (material.pbrData.baseColorTexture.has_value()) {
			std::size_t albedo =
				asset.textures[material.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
			materials[i].albedoTexture = registeredImageIndices[albedo];
		}
		materials[i].albedoValue = (glm::vec3&)material.pbrData.baseColorFactor;

		if (material.normalTexture.has_value()) {
			std::size_t normal = asset.textures[material.normalTexture.value().textureIndex].imageIndex.value();
			materials[i].normalTexture = registeredImageIndices[normal];
		}

		if (material.pbrData.metallicRoughnessTexture.has_value()) {
			std::size_t roughnessMetallic =
				asset.textures[material.pbrData.metallicRoughnessTexture.value().textureIndex].imageIndex.value();
			materials[i].roughnessMetallicTexture = registeredImageIndices[roughnessMetallic];
		}
		materials[i].roughnessValue = material.pbrData.roughnessFactor;
		materials[i].metallicValue = material.pbrData.metallicFactor;
	}
}

void SceneLoader::beginBufferLoad(void* stagingAddress, std::vector<std::size_t> registeredImageIndices) {
	std::jthread([&bufferData = m_bufferDataLocations,
				  &asset = m_asset,
				  stagingAddress,
				  &loadedBuffers = m_readyBuffers,
				  &sceneGeometry = m_sceneGeometry,
				  imageIndices = std::move(registeredImageIndices)] {
		auto* vertexAddress = (glm::vec3*)stagingAddress;
		auto* vertexAttributes =
			(VertexAttributes*)((std::byte*)stagingAddress + bufferData[(int)SceneBuffers::VertexAttribute].offset);
		auto* indices = (uint32_t*)((std::byte*)stagingAddress + bufferData[(int)SceneBuffers::Indices].offset);
		auto* transforms = (glm::mat4*)((std::byte*)stagingAddress + bufferData[(int)SceneBuffers::Transforms].offset);

		auto sceneData = loadGeometryBuffers(asset, vertexAddress, vertexAttributes, indices, transforms);

		sceneGeometry.primitiveBounds = std::move(sceneData.primitivesBounds);
		sceneGeometry.size = sceneData.sceneSize;

		auto* materialsAddress = (MaterialDefinitions::PBRInstance*)((std::byte*)stagingAddress +
																	 bufferData[(int)SceneBuffers::Materials].offset);
		loadMaterials(asset, materialsAddress, imageIndices);

		auto inserter = loadedBuffers.getInserter();
		for (int i = 0; i < SceneLoader::SceneBuffersCount; i++) inserter.push(i);
	}).detach();
}

std::vector<std::byte> getRawImageData(
	std::size_t image, const fastgltf::Asset& asset, const std::filesystem::path& assetPath
) {
	return std::visit(
		fastgltf::visitor {
			[&](const fastgltf::sources::URI& uri) {
				auto file_size = std::filesystem::file_size(assetPath / uri.uri.path());
				std::ifstream file { assetPath / uri.uri.path(), std::ios::binary | std::ios::ate };
				assert(file.is_open());

				std::vector<std::byte> rawData(file_size - uri.fileByteOffset);

				file.seekg(uri.fileByteOffset);
				file.read((char*)rawData.data(), file_size - uri.fileByteOffset);
				file.close();

				return rawData;
			},
			[&](const fastgltf::sources::Array& vector) {
				return std::vector<std::byte>(vector.bytes.data(), vector.bytes.data() + vector.bytes.size_bytes());
			},
			[&](fastgltf::sources::BufferView& view) {
				auto& bufferView = asset.bufferViews[view.bufferViewIndex];
				auto& buffer = asset.buffers[bufferView.bufferIndex];
				return std::visit(
					fastgltf::visitor {
						[&](const fastgltf::sources::Array& vector) {
							return std::vector<std::byte>(
								vector.bytes.data(), vector.bytes.data() + vector.bytes.size_bytes()
							);
						},
						[](auto& arg) { return std::vector<std::byte> {}; },
					},
					buffer.data
				);
			},
			[](auto& arg) { return std::vector<std::byte> {}; },
		},

		asset.images[image].data
	);
}

struct ProcessedImageData {
	std::byte* ptr;
	std::size_t width;
	std::size_t height;
};
ProcessedImageData processImage(const std::vector<std::byte>& imageData, TextureUsageType textureUsage) {
	int width, height, channels;

	int expectedChannels = 0;
	switch (textureUsage) {
		case TextureUsage::Albedo:
			expectedChannels = 3;
			break;
		case TextureUsage::AlbedoWithAlpha:
			expectedChannels = 4;
			break;
		case TextureUsage::RoughnessMetallic:
		case TextureUsage::Normal:
			expectedChannels = 3;
			break;
		default:
			expectedChannels = 0;
	}

	std::byte* imageProcessedData = (std::byte*)stbi_load_from_memory(
		(stbi_uc*)imageData.data(), imageData.size(), &width, &height, &channels, expectedChannels
	);

	assert(imageProcessedData);

	// Roughness/metallic compression
	if (textureUsage == TextureUsage::RoughnessMetallic) {
		for (int c = 0; c < width * height; c++) {
			imageProcessedData[c * 2 + 0] = imageProcessedData[c * 3 + 1];
			imageProcessedData[c * 2 + 1] = imageProcessedData[c * 3 + 2];
		}
	} else if (textureUsage == TextureUsage::Normal) {
		for (int c = 0; c < width * height; c++) {
			imageProcessedData[c * 2 + 0] = imageProcessedData[c * 3 + 0];
			imageProcessedData[c * 2 + 1] = imageProcessedData[c * 3 + 1];
		}
	}
	return {
		.ptr = imageProcessedData,
		.width = static_cast<std::size_t>(width),
		.height = static_cast<std::size_t>(height),
	};
}

void SceneLoader::beginImageLoad(void* address) {
	using ImageIndex = std::size_t;

	auto rawDataStack = std::make_shared<ConcurrentStack<ImageIndex>>();
	auto processImageStack = std::make_shared<ConcurrentStack<ImageIndex>>();
	auto compressImageStack = std::make_shared<ConcurrentStack<ImageIndex>>();

	auto rawImageData = std::make_shared<std::vector<std::vector<std::byte>>>(m_asset.images.size());
	auto processedImageData = std::make_shared<std::vector<ProcessedImageData>>(m_asset.images.size());
	auto compressedImagesOffsets = std::make_shared<std::vector<std::size_t>>(m_asset.images.size());

	for (int i = 0; i < m_asset.images.size(); i++) rawDataStack->container().push_back(i);

	for (int i = 0; i < 16; i++) {
		std::jthread([rawDataStack, processImageStack, rawImageData, &asset = m_asset, &path = m_path]() {
			auto stackInserter = processImageStack->getInserter();
			while (auto image = rawDataStack->pop()) {
				(*rawImageData)[image.value()] = getRawImageData(image.value(), asset, path.parent_path());

				stackInserter.push(image.value());
			}
		}).detach();
	}
	for (int i = 0; i < 2; i++) {
		std::jthread([compressImageStack,
					  processImageStack,
					  rawImageData,
					  processedImageData,
					  &textureUsages = m_textureUsages] {
			auto stackInserter = compressImageStack->getInserter();

			while (auto popValue = processImageStack->pop_wait()) {
				auto image = popValue.value();
				auto& data = (*rawImageData)[image];

				(*processedImageData)[image] = processImage(data, textureUsages[image]);

				stackInserter.push(image);
				(*rawImageData)[image] = {};
			}
		}).detach();
	}

	for (int i = 0; i < 4; i++) {
		std::jthread([compressImageStack,
					  &readyImages = m_readyImages,
					  processedImageData,
					  &textureUsages = m_textureUsages,
					  &imageDataLocations = m_imageDataLocations,
					  address] {
			auto stackInserter = readyImages.getInserter();
			while (auto workElement = compressImageStack->pop_wait()) {
				auto image = workElement.value();
				auto format = getFormatFromUsage(textureUsages[image]);
				auto& data = (*processedImageData)[image];

				uint8_t mipLevels = getMipLevels(data.width, data.height);

				texture_compressor::compress(
					data.width,
					data.height,
					format,
					data.ptr,
					static_cast<std::byte*>(address) + imageDataLocations[image].offset,
					mipLevels
				);

				stbi_image_free(data.ptr);

				stackInserter.push(image);
			}
		}).detach();
	}
}

SceneLoader::LoadStatus SceneLoader::queryLoadStatus() {
	SceneLoader::LoadStatus status;

	while (auto processedImage = m_readyImages.pop()) status.loadedImages.push_back(*processedImage);
	while (auto processedBuffer = m_readyBuffers.pop()) status.loadedBuffers.push_back(*processedBuffer);

	return status;
}
SceneLoader::SceneGeometry SceneLoader::getSceneGeometry() { return m_sceneGeometry; }
