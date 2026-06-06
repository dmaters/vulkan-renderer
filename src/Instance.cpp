#include "Instance.hpp"

#include <vulkan/vulkan_core.h>

#include <cstdint>
#include <iostream>
#include <string_view>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;

#include "SDL3/SDL_vulkan.h"
std::optional<Instance> Instance::_instance;

vk::Instance createInstance();
void setupDebug(vk::Instance instance);
vk::PhysicalDevice getPhysicalDevice(vk::Instance instance);
vk::Device createDevice(vk::PhysicalDevice physicalDevice);
Instance::QueueFamilies getQueueFamilies(vk::PhysicalDevice device);

Instance& Instance::Create(SDL_Window* window) {
	VULKAN_HPP_DEFAULT_DISPATCHER.init();
	vk::Instance instance = createInstance();
	VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
	setupDebug(instance);

	VkSurfaceKHR surface;
	bool res = SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface);

	if (res) {
		std::cerr << SDL_GetError() << std::endl;
	}
	vk::PhysicalDevice physicalDevice = getPhysicalDevice(instance);
	vk::Device device = createDevice(physicalDevice);

	VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
	auto formats = physicalDevice.getSurfaceFormatsKHR(surface);

	vk::Queue graphicQueue, transferQueue;

	Instance::QueueFamilies families = getQueueFamilies(physicalDevice);
	graphicQueue = device.getQueue(families.graphicsIndex, 0);

	if (families.transferAvailable)
		transferQueue = device.getQueue(families.transferIndex, 0);
	else
		transferQueue = graphicQueue;

	vk::SurfaceFormatKHR format = formats[0];
	for (vk::SurfaceFormatKHR f : formats) {
		if ((f.format == vk::Format::eB8G8R8A8Srgb ||
		     f.format == vk::Format::eB8G8R8A8Srgb) &&
		    f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			format = f;
			break;
		}
	}

	vk::PhysicalDeviceLimits physicalDeviceLimits =
		physicalDevice.getProperties().limits;
	Instance::PhysicalDeviceLimits limitCache {
		.minStorageBufferOffsetAlignment =
			physicalDeviceLimits.minStorageBufferOffsetAlignment,
		.minUniformBufferOffsetAlignment =
			physicalDeviceLimits.minUniformBufferOffsetAlignment,
	};

	Instance::_instance = {
		.device = device,
		.surface = surface,
		.surfaceFormat = format,
		.physicalDevice = physicalDevice,
		.physicalDeviceLimits = limitCache,
		.instance = instance,
		.graphicQueue = graphicQueue,
		.transferQueue = transferQueue,
		.presentQueue = graphicQueue,
		.queueFamiliesIndices = getQueueFamilies(physicalDevice),
	};
	Instance::_instance->swapchain = Swapchain::Create();

	return _instance.value();
}

//// Instance

const std::vector<const char*> instanceExtensionLayers {
	vk::EXTDebugUtilsExtensionName,
	vk::KHRSurfaceExtensionName,
#ifdef _WIN32
	"VK_KHR_win32_surface",
#elif defined(__linux__)
	"VK_KHR_xlib_surface"
#endif
};

vk::Instance createInstance() {
	vk::ApplicationInfo appInfo {
		.apiVersion = vk::ApiVersion13,
	};

	vk::Instance instance = vk::createInstance(
		vk::InstanceCreateInfo {
			.pApplicationInfo = &appInfo,
			.enabledExtensionCount = (uint32_t)instanceExtensionLayers.size(),
			.ppEnabledExtensionNames = instanceExtensionLayers.data(),
		}
	);

	return instance;
}
//// Debug

// vkbootstrap debug output
std::string_view to_string_message_severity(
	VkDebugUtilsMessageSeverityFlagBitsEXT s
) {
	switch (s) {
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			return "VERBOSE";
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			return "ERROR";
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			return "WARNING";
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			return "INFO";
		default:
			return "UNKNOWN";
	}
}
std::string_view to_string_message_type(VkDebugUtilsMessageTypeFlagsEXT s) {
	if (s == 7) return "General | Validation | Performance";
	if (s == 6) return "Validation | Performance";
	if (s == 5) return "General | Performance";
	if (s == 4 /*VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT*/)
		return "Performance";
	if (s == 3) return "General | Validation";
	if (s == 2 /*VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT*/)
		return "Validation";
	if (s == 1 /*VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT*/)
		return "General";
	return "Unknown";
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
	vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	vk::DebugUtilsMessageTypeFlagsEXT messageType,
	const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void*
) {
	auto ms = to_string_message_severity(
		(VkDebugUtilsMessageSeverityFlagBitsEXT)messageSeverity
	);
	auto mt =
		to_string_message_type((VkDebugUtilsMessageTypeFlagsEXT)messageType);
	printf("[%s: %s]\n%s\n", ms.data(), mt.data(), pCallbackData->pMessage);

	return VK_FALSE;  // Applications must return false here
}
void setupDebug(vk::Instance instance) {
	vk::DebugUtilsMessengerCreateInfoEXT messengerCreateInfo = {
		.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
		.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral,
		.pfnUserCallback = debug_callback,

	};

	auto _ = instance.createDebugUtilsMessengerEXTUnique(messengerCreateInfo);
}

//// PhysicalDevice

vk::PhysicalDevice getPhysicalDevice(vk::Instance instance) {
	std::vector<vk::PhysicalDevice> devices =
		instance.enumeratePhysicalDevices();

	std::optional<vk::PhysicalDevice> integrated;

	for (auto device : devices) {
		vk::PhysicalDeviceProperties properties = device.getProperties();
		vk::PhysicalDeviceFeatures features = device.getFeatures();

		if (!features.geometryShader) continue;
		if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
			return device;
		if (properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu)
			integrated = device;
	}

	if (integrated.has_value()) return *integrated;

	throw "No suitable physical device found.";
}

/// Queue families

Instance::QueueFamilies getQueueFamilies(vk::PhysicalDevice device) {
	Instance::QueueFamilies families;

	std::vector<vk::QueueFamilyProperties> properties =
		device.getQueueFamilyProperties();

	for (int i = 0; i < properties.size(); i++) {
		auto property = properties[i];

		if (property.queueFlags & vk::QueueFlagBits::eGraphics) {
			families.graphicsIndex = i;
			families.presentIndex = i;
		}
		if (property.queueFlags & vk::QueueFlagBits::eTransfer &&
		    !(property.queueFlags & vk::QueueFlagBits::eGraphics)) {
			families.transferIndex = i;
			families.transferAvailable = true;
		}
	}

	if (!families.transferAvailable)
		families.transferIndex = families.graphicsIndex;

	return families;
}

//// Device
const std::vector<const char*> deviceLayers {};
const std::vector<const char*> deviceExtensions {
	"VK_KHR_swapchain",
	"VK_KHR_dynamic_rendering",
	"VK_KHR_depth_stencil_resolve",
	"VK_KHR_create_renderpass2",
	"VK_KHR_multiview",
	"VK_KHR_maintenance2",
	"VK_KHR_synchronization2",
	"VK_EXT_descriptor_indexing",
	"VK_KHR_push_descriptor",
};
vk::Device createDevice(vk::PhysicalDevice physicalDevice) {
	Instance::QueueFamilies queueFamilies = getQueueFamilies(physicalDevice);
	std::vector<vk::DeviceQueueCreateInfo> queueInfo {
		{
         .queueFamilyIndex = (uint32_t)queueFamilies.graphicsIndex,
         .queueCount = 1,
         .pQueuePriorities = std::array<float, 3> { 1.f, 1.f, 1.f }.data(),
		 },
	};

	if (queueFamilies.graphicsIndex != queueFamilies.transferIndex)
		queueInfo.push_back(
			{
				.queueFamilyIndex = (uint32_t)queueFamilies.transferIndex,
				.queueCount = 1,
				.pQueuePriorities =
					std::array<float, 3> { 1.f, 1.f, 1.f }
                      .data()
        }
		);

	vk::PhysicalDeviceVulkan11Features vulkan11Features {
		.shaderDrawParameters = true,
	};

	vk::PhysicalDeviceVulkan12Features vulkan12Features {
		.pNext = &vulkan11Features,
		.descriptorIndexing = true,
		.descriptorBindingSampledImageUpdateAfterBind = true,
		.descriptorBindingUpdateUnusedWhilePending = true,
		.descriptorBindingPartiallyBound = true,
		.descriptorBindingVariableDescriptorCount = true,
		.runtimeDescriptorArray = true,
		.hostQueryReset = true,
		.timelineSemaphore = true,

	};
	vk::PhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeature {
		.pNext = &vulkan12Features,
		.dynamicRendering = true,
	};
	vk::PhysicalDeviceSynchronization2FeaturesKHR syncronizationFeature {
		.pNext = &dynamicRenderingFeature, .synchronization2 = true
	};

	vk::PhysicalDeviceFeatures features {
		.multiDrawIndirect = true,
	};

	vk::DeviceCreateInfo info {
		.pNext = &syncronizationFeature,
		.queueCreateInfoCount = (uint32_t)queueInfo.size(),
		.pQueueCreateInfos = queueInfo.data(),
		.enabledLayerCount = (uint32_t)deviceLayers.size(),
		.ppEnabledLayerNames = deviceLayers.data(),
		.enabledExtensionCount = (uint32_t)deviceExtensions.size(),
		.ppEnabledExtensionNames = deviceExtensions.data(),
		.pEnabledFeatures = &features,
	};

	return physicalDevice.createDevice(info);
}
