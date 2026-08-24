#pragma once

#include <SDL3/SDL_video.h>

#include <filesystem>
#include <vulkan/vulkan.hpp>

#include "Instance.hpp"
#include "material/MaterialManager.hpp"
#include "rendergraph/RenderGraph.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"

class Renderer {
private:
	vk::Queue m_graphicsQueue;
	vk::Queue m_presentQueue;
	vk::CommandPool m_commandPool;
	vk::DescriptorPool m_descriptorPool;

	Instance& m_instance;
	ResourceManager m_resourceManager;
	MaterialManager m_materialManager;
	RenderGraph m_graph;

	std::vector<TaskIndex> m_optionalPasses;

	Scene m_currentScene;

	std::vector<TaskIndex> createRenderGraph(Scene& scene);

public:
	Renderer(SDL_Window* window);
	void load(const std::filesystem::path& path);
	void render();
	void setResolution(int width, int height);
	Scene& getScene() { return m_currentScene; }
};
