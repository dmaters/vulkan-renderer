#pragma once

#include <SDL3/SDL_video.h>

#include <filesystem>
#include <vulkan/vulkan.hpp>

#include "Instance.hpp"
#include "RenderingConfiguration.hpp"
#include "material/MaterialManager.hpp"
#include "rendergraph/RenderGraph.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneManager.hpp"

class Renderer {
private:
	struct StaticResources {
		rendergraph::ResourceIndex vertexBuffer;
		rendergraph::ResourceIndex vertexAttributeBuffer;
		rendergraph::ResourceIndex indexBuffer;
		rendergraph::ResourceIndex pbrMaterialData;
		rendergraph::ResourceIndex pbrMaterialInstances;
	};

	vk::Queue m_graphicsQueue;
	vk::Queue m_presentQueue;
	vk::CommandPool m_commandPool;
	vk::DescriptorPool m_descriptorPool;

	RenderingConfiguration m_configuration;

	Instance& m_instance;
	ResourceManager m_resourceManager;
	MaterialManager m_materialManager;

	RenderGraph m_graph;
	StaticResources m_staticResources;
	struct Passes {
		TaskIndex ui;
		std::vector<TaskIndex> optionalPasses;
	};
	Passes m_passes;
	SceneManager m_sceneManager;
	bool m_loadingScene = false;
	std::size_t m_resourceCount;
	Scene m_scene;

	Renderer::Passes createRenderGraph();

public:
	Renderer(SDL_Window* window);
	void load(const std::filesystem::path& path);
	void render();
	void setResolution(int width, int height);
	glm::ivec2 getResolution() const { return m_configuration.resolution; }

	Scene& getScene() { return m_scene; }
};
