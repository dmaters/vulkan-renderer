#include "Scene.hpp"

#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <assimp/Importer.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <vector>
#include <vulkan/vulkan_enums.hpp>

#include "material/MaterialManager.hpp"
#include "resources/MemoryAllocator.hpp"

Scene Scene::loadMesh(
	const std::filesystem::path& path,
	MemoryAllocator& allocator,
	MaterialManager& materialManager
) {
	Assimp::Importer importer;
	auto importedScene = importer.ReadFile(
		path.string(), aiProcess_Triangulate | aiProcess_JoinIdenticalVertices
	);

	auto mesh = importedScene->mMeshes[0];

	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;

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

	vk::BufferCreateInfo vertexInfo {
		.size = vertexBufferSize,
		.usage = vk::BufferUsageFlagBits::eVertexBuffer |
		         vk::BufferUsageFlagBits::eTransferDst
	};

	auto vertexBuffer = allocator.allocateBuffer(
		vertexInfo, MemoryAllocator::Location::DeviceLocal
	);

	auto vertexData = reinterpret_cast<const std::byte*>(vertices.data());
	std::vector<std::byte> rawVertices(
		vertexData, vertexData + vertexBufferSize
	);
	allocator.copyToBuffer(rawVertices, vertexBuffer);
	size_t indexBufferSize = sizeof(unsigned int) * indices.size();

	vk::BufferCreateInfo indexInfo {
		.size = indexBufferSize,
		.usage = vk::BufferUsageFlagBits::eIndexBuffer |
		         vk::BufferUsageFlagBits::eTransferDst
	};

	auto indexBuffer = allocator.allocateBuffer(
		indexInfo, MemoryAllocator::Location::DeviceLocal
	);

	auto indexData = reinterpret_cast<const std::byte*>(indices.data());
	std::vector<std::byte> rawIndices(indexData, indexData + indexBufferSize);
	allocator.copyToBuffer(rawIndices, indexBuffer);

	Scene scene;

	auto materialDesc = MaterialManager::MaterialDescription::Default(
		{ .albedo = "resources/textures/image.png" }
	);

	scene.m_primitives.push_back(Primitive {
		.vertexBuffer = vertexBuffer,
		.indexBuffer = indexBuffer,
		.material = materialManager.instantiateMaterial(materialDesc),
		.indexCount = indices.size(),
		.modelMatrix = glm::mat4x4(1),

	});

	return scene;
};