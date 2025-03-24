#pragma once

#include <SDL3/SDL_video.h>

#include <filesystem>
#include <memory>
#include <vulkan/vulkan.hpp>

#include "Camera.hpp"
#include "Instance.hpp"
#include "Rendergraph/RenderGraph.hpp"
#include "Scene.hpp"
#include "Swapchain.hpp"
#include "material/MaterialManager.hpp"
#include "memory/MemoryAllocator.hpp"
#include "resources/ResourceManager.hpp"

class Renderer {
public:
private:
	Instance m_instance;
	vk::Queue m_graphicsQueue;
	vk::Queue m_presentQueue;
	vk::CommandPool m_commandPool;
	vk::DescriptorPool m_descriptorPool;

	std::unique_ptr<MemoryAllocator> m_memoryAllocator;
	std::unique_ptr<ResourceManager> m_resourceManager;
	std::unique_ptr<MaterialManager> m_materialManager;
	std::unique_ptr<RenderGraph> m_renderGraph;

	std::unique_ptr<Swapchain> m_swapchain;
	std::unique_ptr<Scene> m_currentScene;

	Camera m_camera;

	GlobalResources* m_globalData;

	void createSwapchain();
	void createRenderGraph();

public:
	Renderer(SDL_Window* window);
	void load(const std::filesystem::path& path);
	void render();

	inline Camera& getCamera() { return m_camera; }
};