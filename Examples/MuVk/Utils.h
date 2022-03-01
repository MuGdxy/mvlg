#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "CreateInfo.h"
#include "Query.h"
#include "FileLoader.h"
namespace MuVk
{
	namespace Utils
	{
		_NODISCARD inline std::vector<VkImage> getSwapchainImages(VkDevice device, VkSwapchainKHR swapchain)
		{
			uint32_t count;
			vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr);
			std::vector<VkImage> swapChainImages(count);
			vkGetSwapchainImagesKHR(device, swapchain, &count, swapChainImages.data());
			return swapChainImages;
		}

		/*! @brief fill VkSwapchainCreateInfoKHR by common config. 
		*	unset:
			{
				.imageSharingMode,
				.queueFamilyIndexCount,
				.pQueueFamilyIndices
			}
			@param <support> Call MuVk::Query::querySwapChainSupport() to get the return struct.
			@param <actualExtent> The window size
			@param <surface> Surface handle
		*/
		_NODISCARD inline VkSwapchainCreateInfoKHR fillSwapchainCreateInfo(
			const Query::SwapChainSupportDetails& support, 
			VkSurfaceKHR surface,
			VkExtent2D actualExtent, 
			VkFormat format = VK_FORMAT_B8G8R8A8_SRGB,
			VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			auto formatColorSpace = MuVk::Query::chooseSwapSurfaceFormat(support.formats, format, colorSpace);
			auto presentMode = MuVk::Query::chooseSwapPresentMode(support.presentModes);
			auto extent = MuVk::Query::chooseSwapExtent(support.capabilities, actualExtent);
			auto imageCount = MuVk::Query::chooseSwapchainImageCount(support.capabilities);

			VkSwapchainCreateInfoKHR createInfo = MuVk::swapchainCreateInfo();
			createInfo.surface = surface;
			createInfo.minImageCount = imageCount;
			createInfo.imageFormat = formatColorSpace.format;
			createInfo.imageColorSpace = formatColorSpace.colorSpace;
			createInfo.imageExtent = extent;
			createInfo.imageArrayLayers = 1;
			createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			createInfo.preTransform = support.capabilities.currentTransform;
			createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
			createInfo.presentMode = presentMode;
			createInfo.clipped = VK_FALSE;
			createInfo.oldSwapchain = VK_NULL_HANDLE;

			return createInfo;
		}

		inline bool checkDeviceExtensionSupport(VkPhysicalDevice device, const std::vector<const char*>& deviceExtensions)
		{
			auto availableExtensions = MuVk::Query::deviceExtensionProperties(device);
			std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
			for (const auto& extension : availableExtensions)
				requiredExtensions.erase(extension.extensionName);
			return requiredExtensions.empty();
		}

		inline uint32_t findMemoryType(VkPhysicalDevice physicalDevice, 
			const VkMemoryRequirements& requirements, VkMemoryPropertyFlags properties)
		{
			VkPhysicalDeviceMemoryProperties memProperties = MuVk::Query::physicalDeviceMemoryProperties(physicalDevice);
			for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
			{
				if (requirements.memoryTypeBits & (1 << i) &&
					(memProperties.memoryTypes[i].propertyFlags & properties) == properties)
				{
					return i;
				}
			}
			throw std::runtime_error("failed to find proper memory type");
		}

		inline void beginSingleTimeCommand(VkDevice device, VkCommandPool commandPool, VkCommandBuffer& commandBuffer)
		{
			VkCommandBufferAllocateInfo allocInfo = MuVk::commandBufferAllocateInfo();
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandPool = commandPool;
			allocInfo.commandBufferCount = 1;
			vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);
			VkCommandBufferBeginInfo beginInfo{};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			vkBeginCommandBuffer(commandBuffer, &beginInfo);
		}

		inline void endSingleTimeCommand(
			VkDevice device, 
			VkQueue queue, 
			VkCommandPool commandPool, 
			VkCommandBuffer commandBuffer,
			VkSubmitInfo* pSubmitInfo = nullptr)
		{
			vkEndCommandBuffer(commandBuffer);
			if (!pSubmitInfo)
			{
				VkSubmitInfo submitInfo = MuVk::submitInfo();
				submitInfo.commandBufferCount = 1;
				submitInfo.pCommandBuffers = &commandBuffer;
				vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
			}
			else vkQueueSubmit(queue, 1, pSubmitInfo, VK_NULL_HANDLE);
			vkQueueWaitIdle(queue);
			vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
		}

		struct SingleTimeCommandGuard
		{
			VkDevice device;
			VkQueue queue;
			VkCommandPool commandPool;
			VkCommandBuffer commandBuffer;
			VkSubmitInfo submitInfo;
			SingleTimeCommandGuard(VkDevice device, VkQueue queue, VkCommandPool commandPool)
				:device(device), queue(queue), commandPool(commandPool)
			{
				beginSingleTimeCommand(device, commandPool, commandBuffer);
				submitInfo = MuVk::submitInfo();
				submitInfo.commandBufferCount = 1;
				submitInfo.pCommandBuffers = &commandBuffer;
			}
			~SingleTimeCommandGuard()
			{
				endSingleTimeCommand(device, queue, commandPool, commandBuffer, &submitInfo);
			}
		};

		inline VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code)
		{
			VkShaderModule shaderModule;
			VkShaderModuleCreateInfo createInfo = MuVk::shaderModuleCreateInfo();
			createInfo.codeSize = code.size();
			createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
			if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
				throw std::runtime_error("fail to create shader module");
			return shaderModule;
		}

		inline VkShaderModule createShaderModule(VkDevice device, const std::string& path)
		{
			return createShaderModule(device, readFile(path));
		}

		inline VkImageSubresourceLayers fillImageSubresourceLayers(
			VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT)
		{
			VkImageSubresourceLayers subresource;
			subresource.aspectMask = aspect;
			subresource.baseArrayLayer = 0;
			subresource.layerCount = 1;
			subresource.mipLevel = 0;
			return subresource;
		}

		inline VkImageSubresourceRange fillImageSubresourceRange(
			VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT)
		{
			VkImageSubresourceRange subresourceRange{};
			subresourceRange.aspectMask = aspect;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.levelCount = 1;
			subresourceRange.layerCount = 1;
			return subresourceRange;
		}
	}
}

#ifdef GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
namespace MuVk
{
	namespace Utils
	{
		/*! @brief Append the glfw required extensions to the extension list
		*/
		void appendGLFWRequiredExtensions(std::vector<const char*>& extensions)
		{
			uint32_t glfwExtensionCount = 0;
			const char** glfwExtensions;
			glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
			for (uint32_t i = 0; i < glfwExtensionCount; ++i) extensions.push_back(glfwExtensions[i]);
		}
	}
}
#endif