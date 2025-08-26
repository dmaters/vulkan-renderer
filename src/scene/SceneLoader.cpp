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
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <iostream>
#include <optional>
#include <unordered_set>
#include <vector>
#include <vulkan/vulkan_enums.hpp>

#include "Primitive.hpp"
#include "assimp/DefaultLogger.hpp"
#include "material/Material.hpp"
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
		auto texcoord = mesh.mTextureCoords[0][i];
		size = fmax(size, vertex.Length());
		vertices.push_back(Vertex {
			{ vertex.x, vertex.y, vertex.z },
			{
             { normal.x, normal.y, normal.z },
             { tangent.x, tangent.y, tangent.z },
             { texcoord.x, texcoord.y },
			 }
        });
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

void SceneLoader::loadMaterials(
	const aiScene& scene, std::filesystem::path& texturePath
) {
	MaterialIndex material = m_materialManager.getMaterialIndex("pbr_deferred");

	uint32_t imageCount = 3;
	std::vector<uint32_t> imageIndices;

	std::vector<ResourceManager::TextureInfo> textures;
	textures.push_back({
		"resources/textures/default_albedo.png",
		vk::Format::eR8G8B8A8Unorm,
	});

	textures.push_back({
		"resources/textures/default_normal.png",
		vk::Format::eR8G8B8A8Unorm,
	});

	textures.push_back({
		"resources/textures/default_metallicRoughness.png",
		vk::Format::eR8G8B8A8Unorm,
	});

	for (uint32_t i = 0; i < scene.mNumMaterials; i++) {
		aiMaterial* materialInstance = scene.mMaterials[i];
		aiString path;

		if (materialInstance->GetTexture(aiTextureType_DIFFUSE, 0, &path) ==
		        aiReturn_SUCCESS &&
		    path.length > 0) {
			textures.push_back({
				texturePath / std::filesystem::path(path.C_Str()),
				vk::Format::eR8G8B8A8Unorm,
			});
			imageIndices.push_back(imageCount++);
		} else {
			imageIndices.push_back(0);
		}
		if (materialInstance->GetTexture(aiTextureType_NORMALS, 0, &path) ==
		        aiReturn_SUCCESS &&
		    path.length > 0) {
			textures.push_back({
				texturePath / std::filesystem::path(path.C_Str()),
				vk::Format::eR8G8Unorm,
			});
			imageIndices.push_back(imageCount++);
		} else {
			imageIndices.push_back(1);
		}
		if (materialInstance->GetTexture(
				aiTextureType_GLTF_METALLIC_ROUGHNESS, 0, &path
			) == aiReturn_SUCCESS &&
		    path.length > 0) {
			textures.push_back({
				texturePath / std::filesystem::path(path.C_Str()),
				vk::Format::eR8G8B8A8Unorm,
			});
			imageIndices.push_back(imageCount++);
		} else {
			imageIndices.push_back(2);
		}
	}
	auto allocation = m_resourceManager.loadSceneTextures(textures);
	m_materialManager.registerTextureGroup(allocation);
	uint32_t instanceCount = scene.mNumMaterials;

	auto materialData = m_materialManager.getMaterial(
		m_materialManager.getMaterialIndex("pbr_deferred")
	);

	m_materialManager.updateMaterialData<MaterialDefinitions::PBRUniforms>(
		material,
		[instanceCount,
	     imageIndices](MaterialDefinitions::PBRUniforms& instances) {
			for (int i = 0; i < instanceCount; i++) {
				instances[i].albedo = imageIndices.at(i * 3);
				instances[i].normal = imageIndices.at(i * 3 + 1);
				instances[i].roughness_metallic = imageIndices.at(i * 3 + 2);
			}
		}
	);
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
			sceneSize = fmax(
				sceneSize, glm::vec3(instance[3]).length() + primitive.size
			);
			instances.push_back(instance);
		}

		uint32_t instanceCount = instances.size() - firstInstance;

		primitive.baseInstance = firstInstance;
		primitive.instanceCount = instanceCount;

		primitive.materials.push_back({
			m_materialManager.getMaterialIndex("pbr_deferred"),
			mesh->mMaterialIndex,
		});
		primitive.materials.push_back({
			m_materialManager.getMaterialIndex("shadow_map"),
			mesh->mMaterialIndex,
		});

		primitives.push_back(primitive);
	}

	m_primitiveManager.addInstances(instances);

	Scene scene {
		.primitives = primitives,
		.size = sceneSize,
	};

	m_primitiveManager.buildBuffers(m_resourceManager);

	return scene;
}