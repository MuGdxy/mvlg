#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <iostream>
#include <set>
//#include <functional>
namespace MuVk
{
	struct Query
	{
		_NODISCARD
		static std::vector<VkExtensionProperties> instanceExtensionProperties()
		{
			uint32_t extensionCount = 0;
			vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
			std::vector<VkExtensionProperties> ext(extensionCount);
			vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, ext.data());
			return ext;
		}
		_NODISCARD
		static std::vector<VkPhysicalDevice> physicalDevices(VkInstance instance)
		{
			uint32_t deviceCount;
			vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
			std::vector<VkPhysicalDevice> devices(deviceCount);
			vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
			return devices;
		}
		_NODISCARD
		static VkPhysicalDeviceProperties physicalDeviceProperties(VkPhysicalDevice device)
		{
			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(device, &properties);
			return properties;
		}
		_NODISCARD
		static VkPhysicalDeviceFeatures physicalDevicePropertiesfeatures(VkPhysicalDevice device)
		{
			VkPhysicalDeviceFeatures features;
			vkGetPhysicalDeviceFeatures(device, &features);
			return features;
		}
		_NODISCARD
		static std::vector<VkQueueFamilyProperties> physicalDeviceQueueFamilyProperties(VkPhysicalDevice device)
		{
			uint32_t propertyCount;
			vkGetPhysicalDeviceQueueFamilyProperties(device, &propertyCount, nullptr);
			std::vector<VkQueueFamilyProperties> queueFamilies(propertyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(device, &propertyCount, queueFamilies.data());
			return queueFamilies;
		}
		_NODISCARD
		static std::vector<VkExtensionProperties> deviceExtensionProperties(VkPhysicalDevice device)
		{
			uint32_t extensionCount;
			vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
			std::vector<VkExtensionProperties> availableExtensions(extensionCount);
			vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
			return availableExtensions;
		}
		_NODISCARD
		static VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties(VkPhysicalDevice device)
		{
			VkPhysicalDeviceMemoryProperties memoryProperties;
			vkGetPhysicalDeviceMemoryProperties(device, &memoryProperties);
			return memoryProperties;
		}
		_NODISCARD
		static VkMemoryRequirements memoryRequirements(VkDevice device, VkBuffer buffer)
		{
			VkMemoryRequirements memoryRequirements;
			vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);
			return memoryRequirements;
		}
		
		struct SwapChainSupportDetails
		{
			VkSurfaceCapabilitiesKHR capabilities;
			std::vector<VkSurfaceFormatKHR> formats;
			std::vector<VkPresentModeKHR> presentModes;
		};
		_NODISCARD
		static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
		{
			SwapChainSupportDetails details;
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
			uint32_t formatCount;
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
			if (formatCount > 0)
			{
				details.formats.resize(formatCount);
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
			}
			uint32_t presentModeCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
			if (presentModeCount > 0)
			{
				details.presentModes.resize(presentModeCount);
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
			}
			return details;
		}
		_NODISCARD
		static bool checkDeviceExtensionSupport(VkPhysicalDevice device, std::vector<std::string>& requiredExtensions)
		{
			auto availableExtensions = deviceExtensionProperties(device);
			std::set<std::string> extensions(requiredExtensions.begin(), requiredExtensions.end());
			for (const auto& extension : availableExtensions)
				extensions.erase(extension.extensionName);
			return requiredExtensions.empty();
		}
		_NODISCARD
		static VkSurfaceFormatKHR chooseSwapSurfaceFormat(
				const std::vector<VkSurfaceFormatKHR>& availableFormats,
				VkFormat format = VK_FORMAT_B8G8R8A8_SRGB,
				VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			for (const auto& availableFormat : availableFormats)
			{
				if (availableFormat.format == format
					&& availableFormat.colorSpace == colorSpace)
						return availableFormat;
			}
			return availableFormats[0];
		}

		_NODISCARD
		static VkPresentModeKHR chooseSwapPresentMode(
			const std::vector<VkPresentModeKHR>& availablePresentModes,
			VkPresentModeKHR presentMode = VK_PRESENT_MODE_MAILBOX_KHR)
		{
			for (const auto& availablePresentMode : availablePresentModes)
			{
				if (availablePresentMode == presentMode)
					return availablePresentMode;
			}
			return VK_PRESENT_MODE_FIFO_KHR;
		}

		static uint32_t Max(uint32_t a, uint32_t b)
		{
			return a > b ? a : b;
		}

		static uint32_t Min(uint32_t a, uint32_t b)
		{
			return a < b ? a : b;
		}

		_NODISCARD
		static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, VkExtent2D actualExtent)
		{
			if (capabilities.currentExtent.width != UINT32_MAX)
				return capabilities.currentExtent;
			else
			{
				actualExtent.width = Max(capabilities.minImageExtent.width,
					Min(capabilities.maxImageExtent.width, actualExtent.width));
				actualExtent.height = Max(capabilities.minImageExtent.height,
					Min(capabilities.maxImageExtent.height, actualExtent.height));
				return actualExtent;
			}
		}

		_NODISCARD
		static uint32_t chooseSwapchainImageCount(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t expectedCount = -1)
		{
			if (expectedCount == -1 || expectedCount <= capabilities.minImageCount)
				expectedCount = capabilities.minImageCount + 1;
			//capabilities.maxImageCount == 0 for no maximum
			if (capabilities.maxImageCount > 0 && expectedCount > capabilities.maxImageCount)
				expectedCount = capabilities.maxImageCount;
			return expectedCount;
		}
	};
}