#pragma once

#include <SDL3/SDL_video.h>

#include <filesystem>
#include <vulkan/vulkan.hpp>

#include "Instance.hpp"
#include "RenderingConfiguration.hpp"
#include "material/MaterialManager.hpp"
#include "rendergraph/FeatureRegister.hpp"
#include "rendergraph/RenderGraph.hpp"
#include "rendergraph/TaskRegister.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneManager.hpp"

class Renderer {
private:
	vk::Queue m_graphicsQueue;
	vk::Queue m_presentQueue;
	vk::CommandPool m_commandPool;
	vk::DescriptorPool m_descriptorPool;

	RenderingConfiguration m_configuration;

	Instance& m_instance;
	ResourceManager m_resourceManager;
	MaterialManager m_materialManager;

	TaskRegister m_taskRegister;
	FeatureRegister m_featureRegister;

	std::optional<RenderGraph> m_graph;
	SceneManager::SharedBufferContainer<rendergraph::ResourceIndex> m_sharedSceneBuffers;
	SceneManager m_sceneManager;
	bool m_loadingScene = false;
	std::size_t m_resourceCount;
	Scene m_scene;


	static void reloadGraphBuffers(
		ResourceManager::AllocationIndex allocation,
		SceneManager::SharedBufferContainer<rendergraph::ResourceIndex> staticResources,
		ResourceManager& resourceManager,
		RenderGraph& rendergraph
	);

	void registerPasses();

public:
	Renderer(SDL_Window* window);
	void load(const std::filesystem::path& path);
	void render();
	void setResolution(int width, int height);
	glm::ivec2 getResolution() const { return m_configuration.resolution; }

	Scene& getScene() { return m_scene; }
};
