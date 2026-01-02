#include "SceneLoader.hpp"

#include <assimp/material.h>
#include <assimp/matrix4x4.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <assimp/vector3.h>

#include <assimp/Importer.hpp>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <vulkan/vulkan_enums.hpp>

#include "Primitive.hpp"
#include "material/MaterialDefinitions.hpp"
#include "material/MaterialManager.hpp"
#include "resources/ResourceManager.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include "PrimitiveManager.hpp"
#include "material/MaterialManager.hpp"

SceneLoader::SceneLoader(
	ResourceManager& resourceManager, MaterialManager& materialManager
) :
	m_resourceManager(resourceManager),
	m_materialManager(materialManager),
	m_primitiveManager() {}

void loadMaterials() {}

glm::mat4 getBaseTransform(aiNode& node, const aiScene& scene) {
	glm::mat4 transform;
	aiMatrix4x4 base = node.mTransformation;

	transform[0][0] = base.a1;
	transform[1][0] = base.a2;
	transform[2][0] = base.a3;
	transform[3][0] = base.a4;
	transform[0][1] = base.b1;
	transform[1][1] = base.b2;
	transform[2][1] = base.b3;
	transform[3][1] = base.b4;
	transform[0][2] = base.c1;
	transform[1][2] = base.c2;
	transform[2][2] = base.c3;
	transform[3][2] = base.c4;
	transform[0][3] = base.d1;
	transform[1][3] = base.d2;
	transform[2][3] = base.d3;
	transform[3][3] = base.d4;

	if (node.mParent == nullptr) return transform;

	return transform * getBaseTransform(*node.mParent, scene);
}

Primitive SceneLoader::loadMesh(aiMesh& mesh) {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	float size = 0;

	for (unsigned int i = 0; i < mesh.mNumFaces; i++) {
		auto face = mesh.mFaces[i];

		indices.push_back(face.mIndices[0]);
		indices.push_back(face.mIndices[1]);
		indices.push_back(face.mIndices[2]);
	}

	for (unsigned int i = 0; i < mesh.mNumVertices; i++) {
		auto vertex = mesh.mVertices[i];
		auto normal = mesh.mNormals[i];
		auto tangent = mesh.mTangents[i];
		auto bitangent = mesh.mBitangents[i];

		auto texcoord = mesh.mTextureCoords[0][i];
		size = fmax(size, vertex.Length());
		vertices.push_back(
			Vertex {
				{ vertex.x,                        vertex.y, vertex.z },
				{
                 { normal.x, normal.y, normal.z },
                 { tangent.x, tangent.y, tangent.z },
                 { bitangent.x, bitangent.y, bitangent.z },
                 { texcoord.x, texcoord.y },
				 }
        }
		);
	};

	uint32_t vertexOffset;
	uint32_t indexOffset;
	m_primitiveManager.addPrimitive(
		vertices, indices, vertexOffset, indexOffset
	);

	return Primitive {
		.baseVertex = vertexOffset,
		.baseIndex = indexOffset,
		.indexCount = (uint32_t)indices.size(),
		.size = size,
	};
}

void SceneLoader::loadNode(aiNode& root, const aiScene& importedScene) {
	if (root.mNumMeshes > 0) {
		glm::mat4 transform = getBaseTransform(root, importedScene);

		for (uint32_t i = 0; i < root.mNumMeshes; i++) {
			m_instanceCache[root.mMeshes[i]].push_back(transform);
		}
	}

	for (int i = 0; i < root.mNumChildren; i++) {
		loadNode(*root.mChildren[i], importedScene);
	}
}

void loadPrimitives() {}

std::vector<MaterialDefinitions::PBRInstance> SceneLoader::loadMaterials(
	const aiScene& scene, std::filesystem::path& texturePath
) {
	uint32_t imageCount = 3;
	std::vector<uint32_t> imageIndices;

	std::vector<ResourceManager::TextureInfo> textures;
	std::vector<MaterialDefinitions::PBRInstance> instances;

	textures.push_back(
		{
			"resources/textures/default_albedo.png",
			vk::Format::eR8G8B8A8Srgb,
		}
	);

	textures.push_back(
		{
			"resources/textures/default_normal.png",
			vk::Format::eR8G8Unorm,
		}
	);

	textures.push_back(
		{
			"resources/textures/default_metallicRoughness.png",
			vk::Format::eR8G8B8A8Srgb,
		}
	);

	for (uint32_t i = 0; i < scene.mNumMaterials; i++) {
		aiMaterial* materialInstance = scene.mMaterials[i];
		aiString path;
		MaterialDefinitions::PBRInstance instance;

		if (materialInstance->GetTexture(aiTextureType_DIFFUSE, 0, &path) ==
		        aiReturn_SUCCESS &&
		    path.length > 0) {
			textures.push_back(
				{
					texturePath / std::filesystem::path(path.C_Str()),
					vk::Format::eR8G8B8A8Srgb,
				}
			);
			instance.albedoTexture = imageCount++;
		}
		aiColor4D diffuseColor;
		if (materialInstance->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseColor) ==
		    AI_SUCCESS)
			instance.albedoValue =
				glm::vec3(diffuseColor.r, diffuseColor.g, diffuseColor.b);

		if (materialInstance->GetTexture(aiTextureType_NORMALS, 0, &path) ==
		        aiReturn_SUCCESS &&
		    path.length > 0) {
			textures.push_back(
				{
					texturePath / std::filesystem::path(path.C_Str()),
					vk::Format::eR8G8Unorm,
				}
			);
			instance.normalTexture = imageCount++;
		} else {
			imageIndices.push_back(1);
		}
		if (materialInstance->GetTexture(
				aiTextureType_GLTF_METALLIC_ROUGHNESS, 0, &path
			) == aiReturn_SUCCESS &&
		    path.length > 0) {
			textures.push_back(
				{
					texturePath / std::filesystem::path(path.C_Str()),
					vk::Format::eR8G8B8A8Unorm,
				}
			);
			instance.roughnessMetallicTexture = imageCount++;
		}
		float metallicValue;
		if (materialInstance->Get(AI_MATKEY_METALLIC_FACTOR, metallicValue) ==
		    AI_SUCCESS)
			instance.metallicValue = metallicValue;

		float roughnessValue;
		if (materialInstance->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughnessValue) ==
		    AI_SUCCESS)
			instance.roughnessValue = roughnessValue;

		instances.push_back(instance);
	}
	auto allocation = m_resourceManager.loadSceneTextures(textures);
	m_materialManager.registerTextureGroup(allocation);

	return instances;
}

Scene SceneLoader::load(const std::filesystem::path& path) {
	Assimp::Importer importer;

	auto import = importer.ReadFile(
		path.string().c_str(),
		aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals
	);

	if (import == nullptr) std::cerr << importer.GetErrorString() << std::endl;
	assert(import != nullptr);

	auto folderPath = path.parent_path();
	std::vector<MaterialDefinitions::PBRInstance> materialInstances =
		loadMaterials(*import, folderPath);

	std::vector<glm::mat4> instances;
	m_instanceCache = std::vector<std::vector<glm::mat4>>(import->mNumMeshes);

	loadNode(*import->mRootNode, *import);

	std::vector<Primitive> primitives;
	primitives.reserve(import->mNumMeshes);

	float sceneSize = 0;
	for (int i = 0; i < import->mNumMeshes; i++) {
		aiMesh* mesh = import->mMeshes[i];
		Primitive primitive = loadMesh(*mesh);

		uint32_t firstInstance = instances.size();

		for (auto instance : m_instanceCache[i]) {
			sceneSize = std::max(
				sceneSize, glm::vec3(instance[3]).length() + primitive.size
			);
			instances.push_back(instance);
		}

		uint32_t instanceCount = instances.size() - firstInstance;

		primitive.baseInstance = firstInstance;
		primitive.instanceCount = instanceCount;

		primitives.push_back(primitive);
	}

	m_primitiveManager.addInstances(instances);

	ResourceManager::AllocationIndex buffersAllocation =
		m_resourceManager.createResources(
			{
    },
			{
				ResourceManager::BufferDescription {
					.size = static_cast<uint32_t>(
						m_primitiveManager.m_positions.size() *
						sizeof(glm::vec3)
					),
					.usage = vk::BufferUsageFlagBits::eVertexBuffer |
	                         vk::BufferUsageFlagBits::eTransferDst,
				},
				ResourceManager::BufferDescription {
					.size = static_cast<uint32_t>(
						m_primitiveManager.m_attributes.size() *
						sizeof(Vertex::Attributes)
					),
					.usage = vk::BufferUsageFlagBits::eVertexBuffer |
	                         vk::BufferUsageFlagBits::eTransferDst,
				},
				ResourceManager::BufferDescription {
					.size = static_cast<uint32_t>(
						m_primitiveManager.m_indexBuffer.size() *
						sizeof(uint32_t)
					),
					.usage = vk::BufferUsageFlagBits::eIndexBuffer |
	                         vk::BufferUsageFlagBits::eTransferDst,
				},
				ResourceManager::BufferDescription {
					.size = static_cast<uint32_t>(
						instances.size() * sizeof(glm::mat4)
					),
					.usage = vk::BufferUsageFlagBits::eVertexBuffer |
	                         vk::BufferUsageFlagBits::eTransferDst,
				},
				ResourceManager::BufferDescription {
					.size = static_cast<uint32_t>(
						materialInstances.size() *
						sizeof(MaterialDefinitions::PBRInstance)
					),
					.usage = vk::BufferUsageFlagBits::eStorageBuffer |
	                         vk::BufferUsageFlagBits::eTransferDst,
				},
			}
		);

	auto& buffers = m_resourceManager.getBuffers(buffersAllocation);

	m_resourceManager.queueBufferUpdate(
		buffers[0],
		[positions = std::move(m_primitiveManager.m_positions)](void* ptr) {
			std::memcpy(
				ptr, positions.data(), positions.size() * sizeof(glm::vec3)
			);
		}
	);
	m_resourceManager.queueBufferUpdate(
		buffers[1],
		[attributes = std::move(m_primitiveManager.m_attributes)](void* ptr) {
			std::memcpy(
				ptr,
				attributes.data(),
				attributes.size() * sizeof(Vertex::Attributes)
			);
		}
	);
	m_resourceManager.queueBufferUpdate(
		buffers[2],
		[indices = std::move(m_primitiveManager.m_indexBuffer)](void* ptr) {
			std::memcpy(ptr, indices.data(), indices.size() * sizeof(uint32_t));
		}
	);
	m_resourceManager.queueBufferUpdate(
		buffers[3],
		[instances = std::move(m_primitiveManager.m_instances)](void* ptr) {
			std::memcpy(
				ptr, instances.data(), instances.size() * sizeof(glm::mat4)
			);
		}
	);
	m_resourceManager.queueBufferUpdate(
		buffers[4], [instances = std::move(materialInstances)](void* ptr) {
			std::memcpy(
				ptr,
				instances.data(),
				instances.size() * sizeof(MaterialDefinitions::PBRInstance)
			);
		}
	);

	m_resourceManager.sync();
	Scene scene {
		.primitives = std::move(primitives),
		.size = sceneSize,
		.allocation = buffersAllocation,
	};

	return scene;
}
