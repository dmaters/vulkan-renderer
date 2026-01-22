#pragma once

#include <SDL3/SDL_video.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "Instance.hpp"
#include "Swapchain.hpp"
#include "material/MaterialManager.hpp"
#include "rendergraph/RenderGraph.hpp"
#include "resources/ResourceManager.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneLoader.hpp"
#include "ui/UI.hpp"

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

	Scene m_currentScene;
	uint32_t m_currentFrame = 0;

	UI::UIParameters m_uiParameters;

	void createRenderGraph(Scene& scene);

public:
	Renderer(SDL_Window* window);
	void load(const std::filesystem::path& path);
	void render();

	Scene& getScene() { return m_currentScene; }
};
