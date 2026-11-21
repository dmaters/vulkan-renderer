#include "Instance.hpp"

#include <cstdint>
#include <iostream>
#include <string_view>
#include <unordered_set>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;

std::optional<Instance> Instance::_instance;

vk::Instance createInstance() {
	vk::ApplicationInfo appInfo {
		.apiVersion = vk::ApiVersion13,
	};

	std::vector<const char*> extNames;
	extNames.push_back(vk::EXTDebugUtilsExtensionName);

	uint32_t extCount;
	auto* extensions = glfwGetRequiredInstanceExtensions(&extCount);

	for (int i = 0; i < extCount; i++) {
		extNames.push_back(extensions[i]);
	}

	auto [status, instance] = vk::createInstance(
		vk::InstanceCreateInfo {
			.pApplicationInfo = &appInfo,
			.enabledExtensionCount = (uint32_t)extNames.size(),
			.ppEnabledExtensionNames = extNames.data(),
		}
	);
	return instance;
}

// vkbootstrap debug output
std::string_view to_string_message_severity(VkDebugUtilsMessageSeverityFlagBitsEXT s) {
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
	if (s == 4 /*VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT*/) return "Performance";
	if (s == 3) return "General | Validation";
	if (s == 2 /*VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT*/) return "Validation";
	if (s == 1 /*VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT*/) return "General";
	return "Unknown";
}

VKAPI_ATTR vk::Bool32 VKAPI_CALL debug_callback(
	vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	vk::DebugUtilsMessageTypeFlagsEXT messageType,
	const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void*
) {
	auto ms = to_string_message_severity((VkDebugUtilsMessageSeverityFlagBitsEXT)messageSeverity);
	auto mt = to_string_message_type((VkDebugUtilsMessageTypeFlagsEXT)messageType);
	printf("[%s: %s]\n%s\n", ms.data(), mt.data(), pCallbackData->pMessage);

	return vk::False;
}
void setupDebug(vk::Instance instance) {
	vk::DebugUtilsMessengerCreateInfoEXT messengerCreateInfo = {
		.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
		.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral,
		.pfnUserCallback = debug_callback,
	};

	auto _ = instance.createDebugUtilsMessengerEXTUnique(messengerCreateInfo);
}

////

vk::PhysicalDevice getPhysicalDevice(vk::Instance instance) {
	auto [status, devices] = instance.enumeratePhysicalDevices();

	std::optional<vk::PhysicalDevice> fallback;

	// Defaults to discrete gpu
	for (auto device : devices) {
		vk::PhysicalDeviceProperties properties = device.getProperties();
		vk::PhysicalDeviceFeatures features = device.getFeatures();

		if (!features.geometryShader) continue;
		if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) return device;
		if (properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu) fallback = device;
	}

	if (fallback.has_value()) return *fallback;

	std::printf("No suitable GPU found");
	abort();
}

Instance::QueueFamilies getQueueFamilies(vk::PhysicalDevice device) {
	Instance::QueueFamilies families;

	std::vector<vk::QueueFamilyProperties> properties = device.getQueueFamilyProperties();

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

	if (!families.transferAvailable) families.transferIndex = families.graphicsIndex;

	return families;
}

//// Device

std::unordered_set<std::string> getSupportedExtensions(vk::PhysicalDevice device) {
	auto [status, properties] = device.enumerateDeviceExtensionProperties();
	std::unordered_set<std::string> extensions;

	for (auto& property : properties) {
		extensions.insert(property.extensionName);
	}
	return extensions;
}

static const std::vector<const char*> _defaultExtensions {
	"VK_KHR_swapchain",			 "VK_KHR_dynamic_rendering",   "VK_KHR_depth_stencil_resolve",
	"VK_KHR_create_renderpass2", "VK_KHR_multiview",		   "VK_KHR_maintenance2",
	"VK_KHR_synchronization2",	 "VK_EXT_descriptor_indexing", "VK_KHR_push_descriptor",
};

static const std::vector<const char*> _raytracingExtensions {
	"VK_KHR_acceleration_structure",
	"VK_KHR_ray_query",
	"VK_KHR_deferred_host_operations",
	"VK_KHR_ray_tracing_pipeline",
};
bool checkRaytracingSupport(const std::unordered_set<std::string>& supportedExtensions) {
	for (auto& extension : _raytracingExtensions) {
		bool supported = supportedExtensions.contains(extension);
		if (!supported) return false;
	}
	return true;
}

struct DeviceInfo {
	vk::Device device;
	bool raytracingSupport;
};
DeviceInfo createDevice(vk::PhysicalDevice physicalDevice) {
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
				.pQueuePriorities = std::array<float, 3> { 1.f, 1.f, 1.f }
					  .data()
		  }
		);

	vk::StructureChain<
		vk::PhysicalDeviceFeatures2,
		vk::PhysicalDeviceVulkan11Features,
		vk::PhysicalDeviceVulkan12Features,
		vk::PhysicalDeviceVulkan13Features,
		vk::PhysicalDeviceRayTracingPipelineFeaturesKHR,
		vk::PhysicalDeviceRayQueryFeaturesKHR,
		vk::PhysicalDeviceAccelerationStructureFeaturesKHR>
		extensionsFeatures = {
			{ .features = { .multiDrawIndirect = true } },
			{ .shaderDrawParameters = true },
			{
				.descriptorIndexing = true,
				.descriptorBindingSampledImageUpdateAfterBind = true,
				.descriptorBindingUpdateUnusedWhilePending = true,
				.descriptorBindingPartiallyBound = true,
				.descriptorBindingVariableDescriptorCount = true,
				.runtimeDescriptorArray = true,
				.hostQueryReset = true,
				.timelineSemaphore = true,
				.bufferDeviceAddress = true,

			 },
			{
				.synchronization2 = true,
				.dynamicRendering = true,
			 },
			{ .rayTracingPipeline = true },
			{ .rayQuery = true },
			{ .accelerationStructure = true },
	};

	std::vector<const char*> extensions;

	extensions.insert(extensions.end(), _defaultExtensions.begin(), _defaultExtensions.end());
	auto supportedExtensions = getSupportedExtensions(physicalDevice);
	bool raytracingSupport = checkRaytracingSupport(supportedExtensions);

	if (raytracingSupport) {
		extensions.insert(extensions.end(), _raytracingExtensions.begin(), _raytracingExtensions.end());
		std::printf("Hardware raytracing supported, raytracing passes enabled.");
	} else {
		extensionsFeatures.unlink<vk::PhysicalDeviceRayTracingPipelineFeaturesKHR>();
		extensionsFeatures.unlink<vk::PhysicalDeviceRayQueryFeaturesKHR>();
		extensionsFeatures.unlink<vk::PhysicalDeviceAccelerationStructureFeaturesKHR>();

		std::printf("Hardware raytracing unsupported, raytracing passes disabled.");
	}

	vk::DeviceCreateInfo info {
		.pNext = extensionsFeatures.get(),
		.queueCreateInfoCount = (uint32_t)queueInfo.size(),
		.pQueueCreateInfos = queueInfo.data(),
		.enabledExtensionCount = (uint32_t)extensions.size(),
		.ppEnabledExtensionNames = extensions.data(),
	};

	auto [status, device] = physicalDevice.createDevice(info);

	return {
		.device = device,
		.raytracingSupport = raytracingSupport,
	};
}

Instance& Instance::Create(GLFWwindow* window) {
	VULKAN_HPP_DEFAULT_DISPATCHER.init();
	vk::Instance instance = createInstance();
	VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
	setupDebug(instance);

	VkSurfaceKHR surface;
	vk::Result res = (vk::Result)glfwCreateWindowSurface(instance, window, nullptr, &surface);

	if (res != vk::Result::eSuccess) {
		const char* error;
		glfwGetError(&error);
		std::cerr << "Couldn't create glfw surface. " << error << std::endl;
		abort();
	}
	vk::PhysicalDevice physicalDevice = getPhysicalDevice(instance);
	auto [device, raytracingSupport] = createDevice(physicalDevice);

	VULKAN_HPP_DEFAULT_DISPATCHER.init(device);
	auto [status, formats] = physicalDevice.getSurfaceFormatsKHR(surface);

	vk::Queue graphicQueue, transferQueue;

	Instance::QueueFamilies families = getQueueFamilies(physicalDevice);
	graphicQueue = device.getQueue(families.graphicsIndex, 0);

	if (families.transferAvailable)
		transferQueue = device.getQueue(families.transferIndex, 0);
	else
		transferQueue = graphicQueue;

	vk::SurfaceFormatKHR format = formats[0];
	for (vk::SurfaceFormatKHR f : formats) {
		if ((f.format == vk::Format::eB8G8R8A8Srgb || f.format == vk::Format::eB8G8R8A8Srgb) &&
			f.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			format = f;
			break;
		}
	}

	vk::PhysicalDeviceLimits physicalDeviceLimits = physicalDevice.getProperties().limits;
	Instance::PhysicalDeviceLimits limitCache {
		.minStorageBufferOffsetAlignment = physicalDeviceLimits.minStorageBufferOffsetAlignment,
		.minUniformBufferOffsetAlignment = physicalDeviceLimits.minUniformBufferOffsetAlignment,
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
		.supportedExtensions = {
		    .raytracing = raytracingSupport,
		}
	};

	Instance::_instance->swapchain = Swapchain::Create();

	return _instance.value();
}
