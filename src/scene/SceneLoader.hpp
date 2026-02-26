#pragma once

#include <assimp/mesh.h>
#include <assimp/scene.h>

#include <filesystem>
#include <unordered_map>

#include "Primitive.hpp"
#include "PrimitiveManager.hpp"
#include "Scene.hpp"
#include "material/Material.hpp"
#include "material/MaterialDefinitions.hpp"
#include "material/MaterialManager.hpp"
#include "resources/ResourceManager.hpp"

class SceneLoader {
	ResourceManager& m_resourceManager;
	MaterialManager& m_materialManager;
	PrimitiveManager m_primitiveManager;

	std::vector<std::vector<glm::mat4>> m_instanceCache;

	struct MeshData {
		Primitive primitive;
		Scene::PrimitiveBound bounds;
	};
	MeshData loadMesh(aiMesh& mesh);
	void loadNode(aiNode& root, const aiScene& importedScene);
	std::optional<ImageHandle> loadTexture(
		aiMaterial* material,
		aiTextureType type,
		std::filesystem::path& folderPath
	);

	struct MaterialData {
		std::vector<MaterialDefinitions::PBRInstance> instances;
		std::unordered_set<uint32_t> alphaTestedInstances;
	};
	MaterialData loadMaterials(
		const aiScene& scene, std::filesystem::path& texturePath
	);

public:
	SceneLoader(
		ResourceManager& resourceManager, MaterialManager& materialManager
	);

	Scene load(const std::filesystem::path& path);
};
