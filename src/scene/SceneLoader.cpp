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

struct VertexAttributes {
	glm::vec3 normal;
	glm::vec3 tangent;
	glm::vec3 bitangent;
	glm::vec2 texcoord;
};

struct Sizes {
	std::size_t vertex = 0;
	std::size_t vertexAttributes = 0;
	std::size_t indices = 0;
	std::size_t transforms = 0;
	std::size_t materialInstances = 0;
	std::size_t materials = 0;
	std::size_t textures = 0;

	std::vector<ResourceManager::ImageDescription> imageDescriptions;
	std::vector<std::size_t> imageBaseSizes;

	std::size_t total = 0;
};

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

std::vector<texture_compressor::Format> getSceneTexturesFormats(
	const fastgltf::Asset& asset
) {
	std::vector<texture_compressor::Format> formats(asset.images.size());

	for (auto& material : asset.materials) {
		if (material.pbrData.baseColorTexture.has_value()) {
			std::size_t baseColor =
				asset.textures[material.pbrData.baseColorTexture->textureIndex]
					.imageIndex.value();

			if (material.alphaMode == fastgltf::AlphaMode::Mask)
				formats[baseColor] = texture_compressor::Format::BC1_ALPHA;
			else
				formats[baseColor] = texture_compressor::Format::BC1;
		}

		if (material.normalTexture.has_value()) {
			std::size_t normal =
				asset.textures[material.normalTexture->textureIndex]
					.imageIndex.value();

			formats[normal] = texture_compressor::Format::BC5;
		}

		if (material.pbrData.metallicRoughnessTexture.has_value()) {
			std::size_t roughnessMetallic =
				asset
					.textures[material.pbrData.metallicRoughnessTexture
			                      ->textureIndex]
					.imageIndex.value();

			formats[roughnessMetallic] = texture_compressor::Format::BC5;
		}
	}

	return formats;
}

uint8_t getMipLevels(int width, int height) {
	uint32_t mipLevels = std::floor(std::log2(std::min(width, height))) + 1;
	mipLevels = mipLevels <= 3 ? 1 : mipLevels - 3;
	return mipLevels;
}

Sizes getStagingAllocationSize(
	const fastgltf::Asset& asset,
	const std::vector<texture_compressor::Format>& formats,
	const std::filesystem::path& assetPath
) {
	// Size

	Sizes sizes;

	for (auto& mesh : asset.nodes) {
		if (!mesh.meshIndex.has_value()) continue;

		for (auto& primitive :
		     asset.meshes[mesh.meshIndex.value()].primitives) {
			std::size_t vertexCount =
				asset
					.accessors[primitive.findAttribute("POSITION")
			                       ->accessorIndex]
					.count;

			sizes.vertex += vertexCount * sizeof(glm::vec3);
			sizes.vertexAttributes += vertexCount * sizeof(VertexAttributes);

			if (primitive.indicesAccessor.has_value()) {
				sizes.indices +=
					asset.accessors[primitive.indicesAccessor.value()].count *
					sizeof(uint32_t);
			} else {
				sizes.indices += vertexCount * sizeof(uint32_t);
			}

			sizes.transforms += sizeof(glm::mat4);
			sizes.materialInstances += sizeof(uint32_t);
		}
	}

	sizes.materials =
		asset.materials.size() * sizeof(MaterialDefinitions::PBRInstance);

	for (int i = 0; i < 3; i++) {
		sizes.imageBaseSizes.push_back(4);
		sizes.imageDescriptions.push_back(
			{
				.width = 1,
				.height = 1,
				.format = vk::Format::eR8G8B8A8Unorm,
				.usage = vk::ImageUsageFlagBits::eTransferDst |
		                 vk::ImageUsageFlagBits::eSampled,
			}
		);
	}
	sizes.textures += 16;

	for (int i = 0; i < asset.images.size(); i++) {
		auto& image = asset.images[i];
		auto* uri = std::get_if<fastgltf::sources::URI>(&image.data);
		if (!uri) continue;

		int width, height, channels;
		stbi_info(
			(assetPath / uri->uri.fspath()).c_str(), &width, &height, &channels
		);

		uint8_t mipLevels = getMipLevels(width, height);
		std::size_t imageSize =
			texture_compressor::query_size(width, height, formats[i]);

		sizes.imageDescriptions.push_back(
			{
				.width = static_cast<uint32_t>(width),
				.height = static_cast<uint32_t>(height),
				.miplevels = mipLevels,
				.format = getFormat(formats[i]),
				.usage = vk::ImageUsageFlagBits::eTransferDst |
		                 vk::ImageUsageFlagBits::eSampled,
			}
		);
		sizes.imageBaseSizes.push_back(imageSize);
		for (int i = 0; i < mipLevels; i++) {
			sizes.textures += imageSize >> (i * 2);
		}
	}

	sizes.total = sizes.vertex + sizes.vertexAttributes + sizes.indices +
	              sizes.transforms + sizes.materials + sizes.materialInstances +
	              sizes.textures;
	return sizes;
}

struct PrimitiveData {
	Scene::PrimitiveBound primitiveBounds;
	std::size_t vertexCount;
	std::size_t indexCount;
};

PrimitiveData loadPrimitiveData(
	const fastgltf::Asset& asset,
	const Sizes& offsets,
	const fastgltf::Primitive& primitive,
	std::size_t vertexOffset,
	std::size_t indexOffset,
	void* stagingAddress
) {
	glm::vec3* vertices = reinterpret_cast<glm::vec3*>(
							  ((std::byte*)stagingAddress) + offsets.vertex
						  ) +
	                      vertexOffset;

	VertexAttributes* vertexAttributes =
		reinterpret_cast<VertexAttributes*>(
			((std::byte*)stagingAddress) + offsets.vertexAttributes
		) +
		vertexOffset;

	uint32_t* indices = reinterpret_cast<uint32_t*>(
							((std::byte*)stagingAddress) + offsets.indices
						) +
	                    indexOffset;

	auto& vertexAccessor =
		asset.accessors[primitive.findAttribute("POSITION")->accessorIndex];
	float size2 = 0;
	glm::vec3 averageVertexPosition = glm::vec3(0);
	fastgltf::iterateAccessorWithIndex<glm::vec3>(
		asset, vertexAccessor, [&](glm::vec3 vertex, std::size_t index) {
			vertices[index] = vertex;
			size2 = std::max(size2, glm::dot(vertex, vertex));
			averageVertexPosition += vertex;
		}
	);
	averageVertexPosition /= vertexAccessor.count;

	std::size_t indexCount = 0;
	if (primitive.indicesAccessor.has_value()) {
		auto& indexAccessor =
			asset.accessors[primitive.indicesAccessor.value()];

		fastgltf::iterateAccessorWithIndex<uint32_t>(
			asset, indexAccessor, [&](uint32_t vertexIndex, std::size_t index) {
				indices[index] = vertexIndex;
			}
		);
		indexCount = indexAccessor.count;
	} else {
		for (int i = 0; i < vertexAccessor.count; i++) {
			indices[indexOffset + i] = vertexOffset + i;
		}
		indexCount = vertexAccessor.count;
	}

	if (primitive.findAttribute("NORMAL") != primitive.attributes.end()) {
		auto& normalAccessor =
			asset.accessors[primitive.findAttribute("NORMAL")->accessorIndex];

		fastgltf::iterateAccessorWithIndex<glm::vec3>(
			asset, normalAccessor, [&](glm::vec3 normal, std::size_t index) {
				vertexAttributes[index].normal = normal;
			}
		);
	} else {
		for (int i = 0; i < indexCount; i += 3) {
			glm::vec3 v1 = vertices[indices[indexOffset + i + 0]];
			glm::vec3 v2 = vertices[indices[indexOffset + i + 1]];
			glm::vec3 v3 = vertices[indices[indexOffset + i + 2]];

			vertexAttributes[indices[indexOffset + i + 0]].normal =
				glm::normalize(glm::cross(v2 - v1, v3 - v1));
			vertexAttributes[indices[indexOffset + i + 1]].normal =
				glm::normalize(glm::cross(v2 - v1, v3 - v1));
			vertexAttributes[indices[indexOffset + i + 2]].normal =
				glm::normalize(glm::cross(v2 - v1, v3 - v1));
		}
	}

	for (int i = 0; i < vertexAccessor.count; i++) {
		// Pixar - Building an Orthonormal Basis, Revisited
		// (2017)

		glm::vec3 normal = vertexAttributes[i].normal;

		float sign = copysign(1.0f, normal.z);
		float a = -1.0f / (sign + normal.z);
		float b = normal.x * normal.y * a;
		vertexAttributes[i].tangent = glm::vec3(
			1.0f + sign * normal.x * normal.x * a, sign * b, -sign * normal.x
		);
		vertexAttributes[i].bitangent =
			glm::vec3(b, sign + normal.y * normal.y * a, -normal.y);
	}

	auto& texcoordAccessor =
		asset.accessors[primitive.findAttribute("TEXCOORD_0")->accessorIndex];
	fastgltf::iterateAccessorWithIndex<glm::vec2>(
		asset, texcoordAccessor, [&](glm::vec2 texcoord, std::size_t index) {
			vertexAttributes[index].texcoord = texcoord;
		}
	);

	return {
	    .primitiveBounds = {
			.position = averageVertexPosition,
			.size = sqrt(size2) - glm::length(averageVertexPosition),
		},
		.vertexCount = vertexAccessor.count,
		.indexCount = indexCount ,
	};
}

struct MeshData {
	std::vector<Primitive> primitives;
	std::vector<Scene::PrimitiveBound> primitivesBounds;
	float sceneSize;

	std::vector<uint32_t> opaquePrimitives;
	std::vector<uint32_t> alphaTestedPrimitives;
};

MeshData loadMeshes(
	const fastgltf::Asset& asset, const Sizes& offsets, void* stagingAddress
) {
	MeshData meshData;
	meshData.primitives.reserve(asset.meshes.size());
	meshData.primitivesBounds.reserve(asset.meshes.size());

	glm::mat4* transforms = reinterpret_cast<glm::mat4*>(
		((std::byte*)stagingAddress) + offsets.transforms
	);

	uint32_t* materialInstances = reinterpret_cast<uint32_t*>(
		((std::byte*)stagingAddress) + offsets.materialInstances
	);

	std::size_t vertexOffset = 0, indexOffset = 0;
	float sceneSize = 0;
	fastgltf::iterateSceneNodes(
		asset,
		0,
		fastgltf::math::fmat4x4(),
		[&](const fastgltf::Node& node, const fastgltf::math::fmat4x4& matrix) {
			if (!node.meshIndex.has_value()) return;

			const fastgltf::Mesh& mesh = asset.meshes[node.meshIndex.value()];
			for (auto& primitive : mesh.primitives) {
				PrimitiveData primitiveData = loadPrimitiveData(
					asset,
					offsets,
					primitive,
					vertexOffset,
					indexOffset,
					stagingAddress
				);

				*materialInstances = primitive.materialIndex.value_or(0);
				materialInstances += 1;

				*transforms = (glm::mat4&)matrix;
				transforms += 1;

				meshData.primitives.push_back(
					{
						.baseVertex = static_cast<uint32_t>(vertexOffset),
						.baseIndex = static_cast<uint32_t>(indexOffset),
						.indexCount =
							static_cast<uint32_t>(primitiveData.indexCount),
					}
				);
				meshData.primitivesBounds.push_back(
					primitiveData.primitiveBounds
				);
				sceneSize =
					std::max(primitiveData.primitiveBounds.size, sceneSize);

				vertexOffset += primitiveData.vertexCount;
				indexOffset += primitiveData.indexCount;

				if (asset.materials[primitive.materialIndex.value()]
			            .alphaMode == fastgltf::AlphaMode::Mask)
					meshData.alphaTestedPrimitives.push_back(
						meshData.primitives.size() - 1
					);
				else
					meshData.opaquePrimitives.push_back(
						meshData.primitives.size() - 1
					);
			}
		}
	);
	meshData.sceneSize = sceneSize;
	return meshData;
}

std::vector<bool> loadMaterialInstances(
	const fastgltf::Asset& asset, const Sizes& offsets, void* stagingAddress
) {
	MaterialDefinitions::PBRInstance* materials =
		reinterpret_cast<MaterialDefinitions::PBRInstance*>(
			((std::byte*)stagingAddress) + offsets.materials
		);
	std::vector<bool> roughnessMetallicImageMap(asset.images.size());

	for (int i = 0; i < asset.materials.size(); i++) {
		auto& material = asset.materials[i];
		materials[i] = {};

		if (material.pbrData.baseColorTexture.has_value()) {
			materials[i].albedoTexture =
				asset
					.textures[material.pbrData.baseColorTexture.value()
			                      .textureIndex]
					.imageIndex.value() +
				3;
		}
		materials[i].albedoValue = (glm::vec3&)material.pbrData.baseColorFactor;

		if (material.normalTexture.has_value())
			materials[i].normalTexture =
				asset.textures[material.normalTexture.value().textureIndex]
					.imageIndex.value() +
				3;

		if (material.pbrData.metallicRoughnessTexture.has_value()) {
			materials[i].roughnessMetallicTexture =
				asset
					.textures[material.pbrData.metallicRoughnessTexture.value()
			                      .textureIndex]
					.imageIndex.value() +
				3;

			roughnessMetallicImageMap
				[materials[i].roughnessMetallicTexture - 3] = true;
		}
		materials[i].roughnessValue = material.pbrData.roughnessFactor;
		materials[i].metallicValue = material.pbrData.metallicFactor;
	}
	return roughnessMetallicImageMap;
}

void loadImages(
	const fastgltf::Asset& asset,
	const Sizes& offsets,
	const std::vector<texture_compressor::Format>& formats,
	const std::vector<bool>& roughnessMetallicImageMap,
	const std::filesystem::path& assetPath,
	void* stagingAddress
) {
	std::byte* ptextures = ((std::byte*)stagingAddress) + offsets.textures;

	// TODO: base images, only add them once
	std::array<uint8_t, 4>* pTexturesAsTex =
		reinterpret_cast<std::array<uint8_t, 4>*>(ptextures);
	pTexturesAsTex[0] = { 255, 255, 255, 255 };  // Color
	pTexturesAsTex[1] = { 128, 128, 255, 255 };  // Normal
	pTexturesAsTex[2] = { 255, 0, 255, 255 };    // Roughness-metallic

	ptextures += 16;  // Aligned

	for (int i = 0; i < asset.images.size(); i++) {
		auto& image = asset.images[i];
		auto* uri = std::get_if<fastgltf::sources::URI>(&image.data);
		assert(uri != nullptr);

		int width, height, channels;
		int expectedChannels = 0;
		switch (formats[i]) {
			case texture_compressor::Format::BC1:
			case texture_compressor::Format::BC5:
				expectedChannels = 3;
				break;
			case texture_compressor::Format::BC1_ALPHA:
				expectedChannels = 4;
				break;
			default:
				expectedChannels = 0;
		}

		std::byte* imageData = reinterpret_cast<std::byte*>(stbi_load(
			(assetPath / uri->uri.path()).c_str(),
			&width,
			&height,
			&channels,
			expectedChannels
		));

		// Roughness/metallic compression
		if (formats[i] == texture_compressor::Format::BC5) {
			if (roughnessMetallicImageMap[i]) {
				for (int i = 0; i < width * height; i++) {
					imageData[i * 2 + 0] = imageData[i * 3 + 1];
					imageData[i * 2 + 1] = imageData[i * 3 + 2];
				}
			} else {
				for (int i = 0; i < width * height; i++) {
					imageData[i * 2 + 0] = imageData[i * 3 + 0];
					imageData[i * 2 + 1] = imageData[i * 3 + 1];
				}
			}
		}
		std::size_t mipLevels = getMipLevels(width, height);
		texture_compressor::compress(
			width, height, formats[i], imageData, ptextures, mipLevels
		);
		stbi_image_free(imageData);
		ptextures += texture_compressor::query_size(
			width, height, formats[i], mipLevels
		);
	}
}

std::vector<ResourceManager::ResourceCopyInfo> buildAllocationCopyInfo(
	BufferHandle stagingBuffer,
	const std::vector<BufferHandle>& sceneBuffers,
	const std::vector<ImageHandle>& sceneImages,
	const Sizes& sizes,
	const Sizes& offsets
) {
	std::vector<ResourceManager::ResourceCopyInfo> copies;

	copies.push_back(
		{
			.source =
				ResourceManager::ResourceCopyInfo::BufferReference {
																	.handle = stagingBuffer,
																	.size = static_cast<uint32_t>(sizes.vertex),
																	.offset = static_cast<uint32_t>(offsets.vertex),
																	},
			.destination = ResourceManager::ResourceCopyInfo::BufferReference {
																	.handle = sceneBuffers[0],
																	.size = static_cast<uint32_t>(sizes.vertex),
																	.offset = 0,

																	}
    }
	);
	copies.push_back(
		{
			.source =
				ResourceManager::ResourceCopyInfo::BufferReference {
																	.handle = stagingBuffer,
																	.size = static_cast<uint32_t>(sizes.vertexAttributes),
																	.offset = static_cast<uint32_t>(offsets.vertexAttributes),
																	},
			.destination = ResourceManager::ResourceCopyInfo::BufferReference {
																	.handle = sceneBuffers[1],
																	.size = static_cast<uint32_t>(sizes.vertexAttributes),
																	.offset = 0,

																	}
    }
	);
	copies.push_back(
		{
			.source =
				ResourceManager::ResourceCopyInfo::BufferReference {
																	.handle = stagingBuffer,
																	.size = static_cast<uint32_t>(sizes.indices),
																	.offset = static_cast<uint32_t>(offsets.indices),
																	},
			.destination = ResourceManager::ResourceCopyInfo::BufferReference {
																	.handle = sceneBuffers[2],
																	.size = static_cast<uint32_t>(sizes.indices),
																	.offset = 0,

																	}
    }
	);
	copies.push_back(
		{
			.source =
				ResourceManager::ResourceCopyInfo::BufferReference {
																	.handle = stagingBuffer,
																	.size = static_cast<uint32_t>(sizes.transforms),
																	.offset = static_cast<uint32_t>(offsets.transforms),
																	},
			.destination = ResourceManager::ResourceCopyInfo::BufferReference {
																	.handle = sceneBuffers[3],
																	.size = static_cast<uint32_t>(sizes.transforms),
																	.offset = 0,

																	}
    }
	);
	copies.push_back(
		{
			.source =
				ResourceManager::ResourceCopyInfo::BufferReference {
																	.handle = stagingBuffer,
																	.size = static_cast<uint32_t>(sizes.materialInstances),
																	.offset = static_cast<uint32_t>(offsets.materialInstances),
																	},
			.destination = ResourceManager::ResourceCopyInfo::BufferReference {
																	.handle = sceneBuffers[4],
																	.size = static_cast<uint32_t>(sizes.materialInstances),
																	.offset = 0,

																	}
    }
	);
	copies.push_back(
		{
			.source =
				ResourceManager::ResourceCopyInfo::BufferReference {
																	.handle = stagingBuffer,
																	.size = static_cast<uint32_t>(sizes.materials),
																	.offset = static_cast<uint32_t>(offsets.materials),
																	},
			.destination = ResourceManager::ResourceCopyInfo::BufferReference {
																	.handle = sceneBuffers[5],
																	.size = static_cast<uint32_t>(sizes.materials),
																	.offset = 0,

																	}
    }
	);

	uint32_t textureBufferOffset = 0;
	for (int i = 0; i < 3; i++) {
		copies.push_back(
			{
				.source =
					ResourceManager::ResourceCopyInfo::BufferReference {
																		.handle = stagingBuffer,
																		.size = 4,
																		.offset = static_cast<uint32_t>(offsets.textures) +
		                          textureBufferOffset, },
				.destination =
					ResourceManager::ResourceCopyInfo::ImageReference {
																		.handle = sceneImages[i],
																		.mipLevel = 0,
																		.initialLayout = vk::ImageLayout::eUndefined,
																		.finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
																		}
        }
		);
		textureBufferOffset += 4;
	}
	textureBufferOffset += 4;  // Alignment to 16
	for (int i = 3; i < sizes.imageDescriptions.size(); i++) {
		for (int mip = 0; mip < sizes.imageDescriptions[i].miplevels; mip++) {
			std::size_t mipSize = sizes.imageBaseSizes[i] >> (mip * 2);
			copies.push_back(
				{
					.source =
						ResourceManager::ResourceCopyInfo::BufferReference {
																			.handle = stagingBuffer,
																			.size = static_cast<uint32_t>(mipSize),
																			.offset = static_cast<uint32_t>(offsets.textures) +
			                          textureBufferOffset, },
					.destination =
						ResourceManager::ResourceCopyInfo::ImageReference {
																			.handle = sceneImages[i],
																			.mipLevel = static_cast<uint32_t>(mip),
																			.initialLayout = vk::ImageLayout::eUndefined,
																			.finalLayout =
								vk::ImageLayout::eShaderReadOnlyOptimal,
																			}
            }
			);
			textureBufferOffset += mipSize;
		}
	}
	return copies;
}

Scene SceneLoader::load(const std::filesystem::path& path) {
	fastgltf::Parser parser;

	auto data = fastgltf::GltfDataBuffer::FromPath(path);
	auto asset = parser.loadGltf(
		data.get(), path.parent_path(), fastgltf::Options::LoadExternalBuffers
	);
	auto formats = getSceneTexturesFormats(asset.get());
	Sizes sizes =
		getStagingAllocationSize(asset.get(), formats, path.parent_path());

	auto stagingAllocation = m_resourceManager.createResources(
		{
    },
		{ {
			.size = static_cast<uint32_t>(sizes.total),
			.usage = vk::BufferUsageFlagBits::eTransferSrc,
		} },
		ResourceManager::MemoryLocation::Host
	);

	Buffer& stagingBuffer = m_resourceManager.getBuffer(
		m_resourceManager.getBuffers(stagingAllocation)[0]
	);

	Sizes offsets;
	offsets.textures = 0;
	offsets.vertex = offsets.textures + sizes.textures;
	offsets.vertexAttributes = offsets.vertex + sizes.vertex;
	offsets.indices = offsets.vertexAttributes + sizes.vertexAttributes;
	offsets.transforms = offsets.indices + sizes.indices;
	offsets.materialInstances = offsets.transforms + sizes.transforms;
	offsets.materials = offsets.materialInstances + sizes.materialInstances;

	auto roughnessMetallicImageMap =
		loadMaterialInstances(asset.get(), offsets, stagingBuffer.data);

	auto meshData = loadMeshes(asset.get(), offsets, stagingBuffer.data);

	loadImages(
		asset.get(),
		offsets,
		formats,
		roughnessMetallicImageMap,
		path.parent_path(),
		stagingBuffer.data
	);

	std::vector<ResourceManager::BufferDescription> buffers {
		{
         .size = static_cast<uint32_t>(sizes.vertex),
         .usage = vk::BufferUsageFlagBits::eTransferDst |
		             vk::BufferUsageFlagBits::eVertexBuffer,
		 },
		{
         .size = static_cast<uint32_t>(sizes.vertexAttributes),
         .usage = vk::BufferUsageFlagBits::eTransferDst |
		             vk::BufferUsageFlagBits::eVertexBuffer,
		 },
		{
         .size = static_cast<uint32_t>(sizes.indices),
         .usage = vk::BufferUsageFlagBits::eTransferDst |
		             vk::BufferUsageFlagBits::eIndexBuffer,
		 },
		{
         .size = static_cast<uint32_t>(sizes.transforms),
         .usage = vk::BufferUsageFlagBits::eTransferDst |
		             vk::BufferUsageFlagBits::eVertexBuffer,
		 },
		{
         .size = static_cast<uint32_t>(sizes.materialInstances),
         .usage = vk::BufferUsageFlagBits::eTransferDst |
		             vk::BufferUsageFlagBits::eStorageBuffer,
		 },
		{
         .size = static_cast<uint32_t>(sizes.materials),
         .usage = vk::BufferUsageFlagBits::eTransferDst |
		             vk::BufferUsageFlagBits::eStorageBuffer,
		 }
	};

	ResourceManager::AllocationIndex sceneAllocation =
		m_resourceManager.createResources(
			sizes.imageDescriptions,
			buffers,
			ResourceManager::MemoryLocation::Device
		);
	auto copies = buildAllocationCopyInfo(
		m_resourceManager.getBuffers(stagingAllocation)[0],
		m_resourceManager.getBuffers(sceneAllocation),
		m_resourceManager.getImages(sceneAllocation),
		sizes,
		offsets
	);

	m_resourceManager.copyResources(copies);

	m_materialManager.registerTextureGroup(sceneAllocation);
	Scene scene {
		.camera = Camera(),
		.lights = {},
		.primitives = meshData.primitives,
		.primitiveBounds = meshData.primitivesBounds,
		.size = meshData.sceneSize,
		.allocation = sceneAllocation,
	};

	MaterialIndex gbufferMaterial =
		m_materialManager.getMaterialIndex("gbuffer");
	MaterialIndex shadowMapMaterial =
		m_materialManager.getMaterialIndex("shadowmap");

	MaterialIndex gbufferAlphaTestedMaterial =
		m_materialManager.getMaterialIndex("gbuffer_alphatested");
	MaterialIndex shadowMapAlphaTestedMaterial =
		m_materialManager.getMaterialIndex("shadowmap_alphatested");

	scene.buckets[gbufferMaterial] = meshData.opaquePrimitives;
	scene.buckets[shadowMapMaterial] = meshData.opaquePrimitives;

	scene.buckets[gbufferAlphaTestedMaterial] = meshData.alphaTestedPrimitives;
	scene.buckets[shadowMapAlphaTestedMaterial] =
		meshData.alphaTestedPrimitives;

	return scene;
}
