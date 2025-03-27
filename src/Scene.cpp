#include "Scene.hpp"

#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <assimp/Importer.hpp>
#include <cstdint>
#include <glm/ext/matrix_float4x4.hpp>
#include <vector>
#include <vulkan/vulkan_enums.hpp>

#include "PrimitiveManager.hpp"
#include "material/MaterialManager.hpp"
#include "resources/ResourceManager.hpp"

Scene Scene::loadMesh(
	const std::filesystem::path& path,
	ResourceManager& resourceManager,
	MaterialManager& materialManager
) {
	Assimp::Importer importer;
	auto importedScene = importer.ReadFile(
		path.string(), aiProcess_Triangulate | aiProcess_JoinIdenticalVertices
	);

	auto mesh = importedScene->mMeshes[0];

	std::vector<Vertex> vertices;
	std::vector<uint16_t> indices;

	for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
		auto face = mesh->mFaces[i];

		indices.push_back(face.mIndices[0]);
		indices.push_back(face.mIndices[1]);
		indices.push_back(face.mIndices[2]);
	}

	for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
		auto vertex = mesh->mVertices[i];
		auto normal = mesh->mNormals[i];
		auto texcoord = mesh->mTextureCoords[0][i];
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
	size_t indexBufferSize = sizeof(uint16_t) * indices.size();

	auto indexData = reinterpret_cast<const std::byte*>(indices.data());
	std::vector<std::byte> rawIndices(indexData, indexData + indexBufferSize);

	Scene scene;

	auto materialDesc = MaterialManager::MaterialDescription::Default(
		{ .albedo = "resources/textures/image.png" }
	);

	PrimitiveManager primitiveManager;

	uint32_t indexOffset, vertexOffset;
	primitiveManager.addPrimitive(
		rawVertices, rawIndices, indexOffset, vertexOffset
	);

	scene.m_primitives.push_back(Primitive {
		.baseVertex = vertexOffset,
		.baseIndex = indexOffset,
		.indexCount = (uint32_t)indices.size(),
		.material = materialManager.instantiateMaterial(materialDesc),
		.modelMatrix = glm::mat4x4(1),
	});

	primitiveManager.buildBuffers(resourceManager);

	return scene;
};