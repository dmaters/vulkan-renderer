#include "SceneLoader.hpp"

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

enum GeometryBuffers {
	Vertex,
	VertexAttribute,
	Indices,
	Transforms,
	MaterialInstances,
	Materials,
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

uint8_t getMipLevels(int width, int height) {
	uint32_t mipLevels = std::floor(std::log2(std::min(width, height))) + 1;
	mipLevels = mipLevels <= 3 ? 1 : mipLevels - 3;
	return mipLevels;
}

struct SceneResourceInfo {
	std::array<std::size_t, 6> geometryBufferSizes { 0 };
	std::size_t buffersAllocationSize = 0;

	std::vector<SceneLoader::SceneResources::MemorySpan> imageDataLocations;
	std::vector<vk::Format> imageFormats;
	std::vector<glm::ivec2> imageResolution;

	std::size_t textureAllocationSize = 0;
};
// Convert the function so that it gives the correct values
SceneResourceInfo getSceneResourceInfo(
	const fastgltf::Asset& asset, const std::filesystem::path& assetPath, const std::vector<uint8_t> textureUsages
) {
	SceneResourceInfo resourceInfo;

	for (auto& mesh : asset.nodes) {
		if (!mesh.meshIndex.has_value()) continue;

		for (auto& primitive : asset.meshes[mesh.meshIndex.value()].primitives) {
			std::size_t vertexCount = asset.accessors[primitive.findAttribute("POSITION")->accessorIndex].count;

			resourceInfo.geometryBufferSizes[GeometryBuffers::Vertex] += vertexCount * sizeof(glm::vec3);
			resourceInfo.geometryBufferSizes[GeometryBuffers::VertexAttribute] +=
				vertexCount * sizeof(VertexAttributes);

			if (primitive.indicesAccessor.has_value()) {
				resourceInfo.geometryBufferSizes[GeometryBuffers::Indices] +=
					asset.accessors[primitive.indicesAccessor.value()].count * sizeof(uint32_t);
			} else {
				resourceInfo.geometryBufferSizes[GeometryBuffers::Indices] += vertexCount * sizeof(uint32_t);
			}

			resourceInfo.geometryBufferSizes[GeometryBuffers::Transforms] += sizeof(glm::mat4);
			resourceInfo.geometryBufferSizes[GeometryBuffers::MaterialInstances] += sizeof(uint32_t);
		}
	}

	resourceInfo.geometryBufferSizes[GeometryBuffers::Materials] =
		asset.materials.size() * sizeof(MaterialDefinitions::PBRInstance);

	for (int i = 0; i < 6; i++) resourceInfo.buffersAllocationSize += resourceInfo.geometryBufferSizes[i];

	for (int i = 0; i < 3; i++) {
		resourceInfo.imageDataLocations.push_back({ .size = 4, .offset = (std::size_t)(4 * i) });
		resourceInfo.imageResolution.push_back({ 1, 1 });
		resourceInfo.imageFormats.push_back(vk::Format::eR8G8B8A8Unorm);
	}
	resourceInfo.textureAllocationSize += 16;

	for (int i = 0; i < asset.images.size(); i++) {
		auto& image = asset.images[i];
		int width, height, channels;

		std::visit(
			fastgltf::visitor {
				[&](const fastgltf::sources::URI& uri) {
					stbi_info((assetPath / uri.uri.path()).string().c_str(), &width, &height, &channels);
				},
				[&](const fastgltf::sources::Array& vector) {
					int width, height, channels;
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

		resourceInfo.imageDataLocations.push_back(
			{
				.size = imageSize,
				.offset = (std::size_t)resourceInfo.textureAllocationSize,
			}
		);

		resourceInfo.imageFormats.push_back(getFormat(compressedFormat));
		resourceInfo.imageResolution.push_back({ width, height });

		for (int i = 0; i < mipLevels; i++) {
			resourceInfo.textureAllocationSize += imageSize >> (i * 2);
		}
	}

	return resourceInfo;
}

struct PrimitiveData {
	Scene::PrimitiveBound primitiveBounds;
	std::size_t vertexCount;
	std::size_t indexCount;
};

PrimitiveData loadPrimitiveData(
	const fastgltf::Asset& asset,
	const fastgltf::Primitive& primitive,
	std::size_t vertexOffset,
	std::size_t indexOffset,
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
			indices[indexOffset + i] = vertexOffset + i;
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
			glm::vec3 v1 = vertices[indices[indexOffset + i + 0]];
			glm::vec3 v2 = vertices[indices[indexOffset + i + 1]];
			glm::vec3 v3 = vertices[indices[indexOffset + i + 2]];

			vertexAttributes[indices[indexOffset + i + 0]].normal = glm::normalize(glm::cross(v2 - v1, v3 - v1));
			vertexAttributes[indices[indexOffset + i + 1]].normal = glm::normalize(glm::cross(v2 - v1, v3 - v1));
			vertexAttributes[indices[indexOffset + i + 2]].normal = glm::normalize(glm::cross(v2 - v1, v3 - v1));
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
	std::vector<Primitive> primitives;
	std::vector<Scene::PrimitiveBound> primitivesBounds;
	float sceneSize;

	std::vector<Scene::MaterialHint> materials;
};

SceneGeometry loadMeshes(
	const fastgltf::Asset& asset,
	glm::vec3* vertices,
	VertexAttributes* vertexAttributes,
	uint32_t* indices,
	glm::mat4* transforms,
	uint32_t* materialInstances
) {
	SceneGeometry meshData;
	meshData.primitives.reserve(asset.meshes.size());
	meshData.primitivesBounds.reserve(asset.meshes.size());

	std::size_t vertexOffset = 0, indexOffset = 0;
	float sceneSize = 0;
	fastgltf::iterateSceneNodes(
		asset, 0, fastgltf::math::fmat4x4(), [&](const fastgltf::Node& node, const fastgltf::math::fmat4x4& matrix) {
			if (!node.meshIndex.has_value()) return;

			const fastgltf::Mesh& mesh = asset.meshes[node.meshIndex.value()];
			for (auto& primitive : mesh.primitives) {
				PrimitiveData primitiveData =
					loadPrimitiveData(asset, primitive, vertexOffset, indexOffset, vertices, vertexAttributes, indices);

				*materialInstances = primitive.materialIndex.value_or(0);
				materialInstances += 1;

				*transforms = (glm::mat4&)matrix;
				transforms += 1;

				meshData.primitives.push_back(
					{
						.baseVertex = static_cast<uint32_t>(vertexOffset),
						.baseIndex = static_cast<uint32_t>(indexOffset),
						.indexCount = static_cast<uint32_t>(primitiveData.indexCount),
					}
				);
				meshData.primitivesBounds.push_back(primitiveData.primitiveBounds);
				sceneSize = std::max(primitiveData.primitiveBounds.size, sceneSize);

				vertexOffset += primitiveData.vertexCount;
				indexOffset += primitiveData.indexCount;

				if (asset.materials[primitive.materialIndex.value()].alphaMode == fastgltf::AlphaMode::Mask)
					meshData.materials.push_back(Scene::MaterialHintBits::Opaque | Scene::MaterialHintBits::AlphaMask);
				else
					meshData.materials.push_back(Scene::MaterialHintBits::Opaque);
			}
		}
	);
	meshData.sceneSize = sceneSize;
	return meshData;
}

void loadMaterialInstances(const fastgltf::Asset& asset, MaterialDefinitions::PBRInstance* materials) {
	for (int i = 0; i < asset.materials.size(); i++) {
		auto& material = asset.materials[i];
		materials[i] = {};

		if (material.pbrData.baseColorTexture.has_value()) {
			materials[i].albedoTexture =
				asset.textures[material.pbrData.baseColorTexture.value().textureIndex].imageIndex.value() + 3;
		}
		materials[i].albedoValue = (glm::vec3&)material.pbrData.baseColorFactor;

		if (material.normalTexture.has_value())
			materials[i].normalTexture =
				asset.textures[material.normalTexture.value().textureIndex].imageIndex.value() + 3;

		if (material.pbrData.metallicRoughnessTexture.has_value()) {
			materials[i].roughnessMetallicTexture =
				asset.textures[material.pbrData.metallicRoughnessTexture.value().textureIndex].imageIndex.value() + 3;
		}
		materials[i].roughnessValue = material.pbrData.roughnessFactor;
		materials[i].metallicValue = material.pbrData.metallicFactor;
	}
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

SceneLoader::SceneResources SceneLoader::querySceneResources() {
	fastgltf::Parser parser;
	auto data = fastgltf::GltfDataBuffer::FromPath(m_path);
	auto asset = parser.loadGltf(data.get(), m_path.parent_path(), fastgltf::Options::LoadExternalBuffers);
	m_textureUsages = getTextureUsages(asset.get());
	SceneResourceInfo resourceInfo = getSceneResourceInfo(asset.get(), m_path.parent_path(), m_textureUsages);

	std::array<SceneResources::MemorySpan, 6> bufferDataLocations;

	std::size_t offset = 0;
	for (int i = 0; i < bufferDataLocations.size(); i++) {
		bufferDataLocations[i] = {
			.size = resourceInfo.geometryBufferSizes[i],
			.offset = offset,
		};

		offset += resourceInfo.geometryBufferSizes[i];
	}

	m_bufferDataLocations = bufferDataLocations;
	m_imageDataLocations = resourceInfo.imageDataLocations;

	return SceneResources {
		.bufferDataLocations = bufferDataLocations,
		.imageDataLocations = resourceInfo.imageDataLocations,
		.imageFormats = resourceInfo.imageFormats,
		.imageResolution = resourceInfo.imageResolution,
		.buffersStagingSize = resourceInfo.buffersAllocationSize,
		.imageStagingSize = resourceInfo.textureAllocationSize,
	};
}

void SceneLoader::beginBufferLoad(void* stagingAddress) {
	std::jthread([&bufferData = m_bufferDataLocations,
				  &asset = m_asset,
				  stagingAddress,
				  &loaded = m_buffersLoaded,
				  &scene = m_scene] {
		auto* vertexAddress = (glm::vec3*)stagingAddress;
		auto* vertexAttributes =
			(VertexAttributes*)((std::byte*)stagingAddress + bufferData[GeometryBuffers::VertexAttribute].offset);
		auto* indices = (uint32_t*)((std::byte*)stagingAddress + bufferData[GeometryBuffers::Indices].offset);
		auto* transforms = (glm::mat4*)((std::byte*)stagingAddress + bufferData[GeometryBuffers::Transforms].offset);
		auto* materialInstances =
			(uint32_t*)((std::byte*)stagingAddress + bufferData[GeometryBuffers::MaterialInstances].offset);

		auto sceneData = loadMeshes(asset, vertexAddress, vertexAttributes, indices, transforms, materialInstances);

		scene.primitives = std::move(sceneData.primitives);
		scene.primitiveBounds = std::move(sceneData.primitivesBounds);
		scene.materialHint = std::move(sceneData.materials);

		scene.size = sceneData.sceneSize;

		auto* materialsAddress = (MaterialDefinitions::PBRInstance*)((std::byte*)stagingAddress +
																	 bufferData[GeometryBuffers::Materials].offset);
		loadMaterialInstances(asset, materialsAddress);
		loaded = true;
	}).detach();
}

void SceneLoader::beginImageLoad(void* address) {
	auto stagingTextures = reinterpret_cast<std::array<uint8_t, 4>*>((std::byte*)address);

	stagingTextures[0] = { 255, 255, 255, 255 };
	stagingTextures[1] = { 128, 128, 255, 255 };
	stagingTextures[2] = { 255, 0, 0, 255 };
	using ImageIndex = std::size_t;

	auto rawDataStack = std::make_shared<ConcurrentStack<ImageIndex>>();
	auto processImageStack = std::make_shared<ConcurrentStack<ImageIndex>>();
	auto compressImageStack = std::make_shared<ConcurrentStack<ImageIndex>>();
	auto readyImages = std::make_shared<ConcurrentStack<ImageIndex>>();

	auto rawImageData = std::make_shared<std::vector<std::vector<std::byte>>>();
	auto processedImageData = std::make_shared<std::vector<ProcessedImageData>>();
	auto compressedImagesOffsets = std::make_shared<std::vector<std::size_t>>();

	auto& textureUsages = reinterpret_cast<std::vector<TextureUsage>&>(m_textureUsages);

	for (int i = 0; i < m_asset.images.size(); i++) rawDataStack->container().push_back(i);

	for (int i = 0; i < 16; i++) {
		std::jthread([rawDataStack, processImageStack, rawImageData, &asset = m_asset, &path = m_path]() {
			auto stackInserter = processImageStack->getInserter();
			while (auto image = rawDataStack->pop()) {
				(*rawImageData)[image.value()] = getRawImageData(image.value(), asset, path);

				stackInserter.push(image.value());
			}
		}).detach();
	}
	for (int i = 0; i < 2; i++) {
		std::jthread([compressImageStack, processImageStack, rawImageData, processedImageData, &textureUsages] {
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
					  readyImages,
					  processedImageData,
					  &textureUsages,
					  &imageDataLocations = m_imageDataLocations,
					  address] {
			auto stackInserter = readyImages->getInserter();
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
	m_readyImages = readyImages;
}

SceneLoader::LoadStatus SceneLoader::queryLoadStatus() {
	SceneLoader::LoadStatus status;

	while (auto processedImage = m_readyImages->pop()) status.imageLoadedDelta.push_back(*processedImage);

	status.buffersLoaded = m_buffersLoaded;
	return status;
}
Scene SceneLoader::getScene() && { return m_scene; }
