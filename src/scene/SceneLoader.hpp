#pragma once

#include <filesystem>

#include "Scene.hpp"
#include "material/MaterialManager.hpp"
#include "resources/ResourceManager.hpp"

class SceneLoader {
	ResourceManager& m_resourceManager;
	MaterialManager& m_materialManager;

public:
	SceneLoader(
		ResourceManager& resourceManager, MaterialManager& materialManager
	) :
		m_resourceManager(resourceManager),
		m_materialManager(materialManager) {}

	Scene load(const std::filesystem::path& path);
};
