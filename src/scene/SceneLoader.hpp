#pragma once

#include <assimp/mesh.h>
#include <assimp/scene.h>

#include <filesystem>

#include "Primitive.hpp"
#include "PrimitiveManager.hpp"
#include "Scene.hpp"
#include "material/MaterialManager.hpp"
#include "resources/ResourceManager.hpp"

class SceneLoader {
	ResourceManager& m_resourceManager;
	MaterialManager& m_materialManager;
	PrimitiveManager m_primitiveManager;

	Primitive loadMesh(aiMesh& mesh);
	std::vector<Primitive> loadNode(aiNode& root, const aiScene& importedScene);
	std::optional<ImageHandle> loadTexture(
		aiMaterial* material,
		aiTextureType type,
		std::filesystem::path& folderPath
	);
	void loadMaterials(
		const aiScene& scene, std::filesystem::path& texturePath
	);

public:
	SceneLoader(
		ResourceManager& resourceManager, MaterialManager& materialManager
	);
	Scene load(const std::filesystem::path& path);
};
