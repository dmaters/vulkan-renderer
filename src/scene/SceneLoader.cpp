#include "SceneLoader.hpp"

#include <assimp/material.h>
#include <assimp/matrix4x4.h>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/types.h>
#include <assimp/vector3.h>

#include <assimp/Importer.hpp>
#include <cstddef>
#include <cstdint>
#include <glm/ext/matrix_float4x4.hpp>
#include <optional>
#include <vector>
#include <vulkan/vulkan_enums.hpp>

#include "Primitive.hpp"
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

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texcoord;
};

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

	for (unsigned int i = 0; i < mesh.mNumFaces; i++) {
		auto face = mesh.mFaces[i];

		indices.push_back(face.mIndices[0]);
		indices.push_back(face.mIndices[1]);
		indices.push_back(face.mIndices[2]);
	}

	for (unsigned int i = 0; i < mesh.mNumVertices; i++) {
		auto vertex = mesh.mVertices[i];
		auto normal = mesh.mNormals[i];
		auto texcoord = mesh.mTextureCoords[0][i];
		vertices.push_back(Vertex {
			{ vertex.x, vertex.y, vertex.z },
			{ normal.x, normal.y, normal.z },
			{ texcoord.x, texcoord.y }
        });
	};

	size_t vertexBufferSize = sizeof(Vertex) * vertices.size();

	auto vertexData = reinterpret_cast<const std::byte*>(vertices.data());
	std::vector<std::byte> rawVertices(
		vertexData, vertexData + vertexBufferSize
	);
	uint32_t indexBufferSize = sizeof(uint32_t) * indices.size();

	auto indexData = reinterpret_cast<const std::byte*>(indices.data());
	std::vector<std::byte> rawIndices(indexData, indexData + indexBufferSize);

	uint32_t vertexOffset;
	uint32_t indexOffset;
	m_primitiveManager.addPrimitive(
		rawVertices, rawIndices, vertexOffset, indexOffset
	);

	return Primitive {
		.baseVertex = vertexOffset / (uint32_t)sizeof(Vertex),
		.baseIndex = indexOffset / (uint32_t)sizeof(uint32_t),
		.indexCount = (uint32_t)indices.size(),
	};
}

std::vector<Primitive> SceneLoader::loadNode(
	aiNode& root, const aiScene& importedScene
) {
	std::vector<Primitive> nodePrimitives;

	if (root.mNumMeshes > 0) {
		glm::mat4 transform = getBaseTransform(root, importedScene);

		for (uint32_t i = 0; i < root.mNumMeshes; i++) {
			aiMesh* mesh = importedScene.mMeshes[root.mMeshes[i]];
			Primitive primitive = loadMesh(*mesh);

			primitive.material.instanceIndex = mesh->mMaterialIndex;
			primitive.modelMatrix = transform;
			nodePrimitives.push_back(primitive);
		}
	}

	for (int i = 0; i < root.mNumChildren; i++) {
		auto primitives = loadNode(*root.mChildren[i], importedScene);
		nodePrimitives.insert(
			nodePrimitives.end(), primitives.begin(), primitives.end()
		);
	}
	return nodePrimitives;
}

void SceneLoader::loadMaterials(
	const aiScene& scene, std::filesystem::path& texturePath
) {
	std::set<uint32_t> materialCache;
	// std::unordered_map<aiString, ImageHandle> textureCache;

	for (uint32_t i = 0; i < scene.mNumMaterials; i++) {
		aiMaterial* materialInstance = scene.mMaterials[i];
		aiString path;
		MaterialDescription::DefaultMaterialTextures defaultTextures;

		if (materialInstance->GetTexture(aiTextureType_DIFFUSE, 0, &path) ==
		        aiReturn_SUCCESS &&
		    path.length > 0) {
			defaultTextures.albedo =
				texturePath / std::filesystem::path(path.C_Str());
		}

		auto defaultDescription = MaterialDescription::Default(defaultTextures);
		m_materialManager.instantiateMaterial(defaultDescription);
	}
}

Scene SceneLoader::load(const std::filesystem::path& path) {
	Assimp::Importer importer;
	auto import = importer.ReadFile(
		path.string().c_str(), aiProcessPreset_TargetRealtime_Quality
	);
	auto folderPath = path.parent_path();
	loadMaterials(*import, folderPath);
	Scene scene;
	std::vector<Primitive> primitives = loadNode(*import->mRootNode, *import);

	scene.m_primitives = primitives;
	m_primitiveManager.buildBuffers(m_resourceManager);

	return scene;
}