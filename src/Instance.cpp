#include <Instance.hpp>
#include <cstdint>
#include <iostream>
#include <string_view>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;

#include "SDL3/SDL_vulkan.h"

vk::Instance createInstance();
void setupDebug(vk::Instance instance);
vk::PhysicalDevice getPhysicalDevice(vk::Instance instance);
vk::Device createDevice(vk::PhysicalDevice physicalDevice);
Instance::QueueFamilies getQueueFamilies(vk::PhysicalDevice device);

Instance Instance::Create(SDL_Window* window) {
	VULKAN_HPP_DEFAULT_DISPATCHER.init();
	vk::Instance instance = createInstance();
	VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
	setupDebug(instance);

	VkSurfaceKHR surface;
	bool res = SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface);

	if (!res) {
		std::cerr << SDL_GetError() << std::endl;
	}
	vk::PhysicalDevice physicalDevice = getPhysicalDevice(instance);
	vk::Device device = createDevice(physicalDevice);

	VULKAN_HPP_DEFAULT_DISPATCHER.init(device);

	return {
		.device = device,
		.surface = surface,
		.physicalDevice = physicalDevice,
		.instance = instance,
		.queueFamiliesIndices = getQueueFamilies(physicalDevice),
	};
}

//// Instance
const std::vector<const char*> instanceValidationLayers = {
	"VK_LAYER_KHRONOS_validation"
};
const std::vector<const char*> instanceExtensionLayers {
	vk::EXTDebugUtilsExtensionName, "VK_KHR_surface", "VK_KHR_win32_surface"
};
vk::Instance createInstance() {
	vk::Instance instance = vk::createInstance(vk::InstanceCreateInfo {
		.enabledLayerCount = (uint32_t)instanceValidationLayers.size(),
		.ppEnabledLayerNames = instanceValidationLayers.data(),
		.enabledExtensionCount = (uint32_t)instanceExtensionLayers.size(),
		.ppEnabledExtensionNames = instanceExtensionLayers.data(),
	});

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

VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void*) {
	auto ms = to_string_message_severity(messageSeverity);
	auto mt = to_string_message_type(messageType);
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

bool isDeviceSuitable(vk::PhysicalDevice device) {
	vk::PhysicalDeviceProperties properties = device.getProperties();
	vk::PhysicalDeviceFeatures features = device.getFeatures();
	return properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu &&
	       features.geometryShader;
}

vk::PhysicalDevice getPhysicalDevice(vk::Instance instance) {
	std::vector<vk::PhysicalDevice> devices =
		instance.enumeratePhysicalDevices();

	for (auto device : devices)
		if (isDeviceSuitable(device)) return device;

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
		if (property.queueFlags & vk::QueueFlagBits::eTransfer)
			families.transferIndex = i;
	}

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
	"VK_KHR_synchronization2"
};
vk::Device createDevice(vk::PhysicalDevice physicalDevice) {
	Instance::QueueFamilies queueFamilies = getQueueFamilies(physicalDevice);
	vk::DeviceQueueCreateInfo queueInfo[] {
		vk::DeviceQueueCreateInfo {
								   .queueFamilyIndex = queueFamilies.graphicsIndex,
								   .queueCount = 3,
								   .pQueuePriorities = std::array<float,            3> { 1.f, 1.f, 1.f }.data(),
								   },
		vk::DeviceQueueCreateInfo {
								   .queueFamilyIndex = queueFamilies.transferIndex,
								   .queueCount = 1,
								   .pQueuePriorities = std::array<float, 1> { 1.f }.data(),
								   }
	};

	vk::PhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeature {
		.dynamicRendering = true,
	};
	vk::PhysicalDeviceSynchronization2FeaturesKHR syncronizationFeature {
		.pNext = &dynamicRenderingFeature, .synchronization2 = true
	};

	vk::DeviceCreateInfo info {
		.pNext = &syncronizationFeature,
		.queueCreateInfoCount = 2,
		.pQueueCreateInfos = queueInfo,
		.enabledLayerCount = (uint32_t)deviceLayers.size(),
		.ppEnabledLayerNames = deviceLayers.data(),
		.enabledExtensionCount = (uint32_t)deviceExtensions.size(),
		.ppEnabledExtensionNames = deviceExtensions.data(),
	};

	return physicalDevice.createDevice(info);
}