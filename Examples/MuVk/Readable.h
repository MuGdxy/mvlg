#include <vulkan/vulkan.h>
#include <iostream>
#include <vector>
#include "Query.h"
struct tabs
{
	int t;
	tabs(int i) :t(i) {}

	friend std::ostream& operator << (std::ostream& o, const tabs& t)
	{
		for (int i = 0; i < t.t; ++i) o << '\t';
		return o;
	}
};

inline void readableAPIVersion(std::ostream& o, uint32_t version)
{
	o << VK_API_VERSION_VARIANT(version) << '.'
		<< VK_API_VERSION_MAJOR(version) << '.'
		<< VK_API_VERSION_MINOR(version) << '.'
		<< VK_API_VERSION_PATCH(version);
}

inline std::ostream& operator << (std::ostream& o, const std::vector<VkExtensionProperties>& properties)
{
	o << "available extensions:";
	tabs t(1);
	for (const auto& extension : properties)
	{
		o << "\n" << t << extension.extensionName
			<< "<Version=";
		readableAPIVersion(o, extension.specVersion);
		o << ">";
	}
	return o;
}

inline std::ostream& operator << (std::ostream& o, VkPhysicalDevice device)
{
	auto properties = MuVk::Query::physicalDeviceProperties(device);
	o << properties.deviceName;
	return o;
}

inline std::ostream& operator << (std::ostream& o, const std::vector<VkPhysicalDevice>& devices)
{
	o << "physical devices:";
	int index = 0;
	tabs t(1);
	for (const auto& device : devices)
		std::cout << "\n" << t << "[" << index++ << "] " << device;
	return o;
}

inline std::ostream& operator << (std::ostream& o, const std::vector<VkQueueFamilyProperties>& families)
{
	o << "queue families:";
	auto presentFlags = [](std::ostream& o, VkQueueFlags flags)
	{

		o << tabs(1) << "Flags:";
		if (flags & VkQueueFlagBits::VK_QUEUE_GRAPHICS_BIT) o << "[Graphics]";
		if (flags & VkQueueFlagBits::VK_QUEUE_COMPUTE_BIT) o << "[Compute]";
		if (flags & VkQueueFlagBits::VK_QUEUE_TRANSFER_BIT) o << "[Transfer]";
		if (flags & VkQueueFlagBits::VK_QUEUE_SPARSE_BINDING_BIT) o << "[Sparse Binding]";
	};
	int index = 0;
	tabs t(1);
	for (const auto& family : families)
	{
		o << "\n" << t << "family index=" << index++;
		o << "\n" << t << "Queue Count=" << family.queueCount << "\n";
		presentFlags(o, family.queueFlags);
	}
	return o;
}

inline std::ostream& operator << (std::ostream& o, const VkMemoryRequirements& requirements)
{
	tabs t(1);
	o << "memory requirements:\n";
	o << t << "alignment=" << requirements.alignment << "\n";
	o << t << "size=" << requirements.size << "\n";
	o << t << "type bits=0x" << std::uppercase << std::hex << requirements.memoryTypeBits << std::dec;
	return o;
}

inline std::ostream& operator << (std::ostream& o, const VkPhysicalDeviceMemoryProperties& properties)
{
	auto presentType = [](std::ostream& o, const VkMemoryType& type)
	{
		tabs t(1);
		o << t << "heap index=" << type.heapIndex << "\n";
		o << t << "propertyFlags: ";
		if (type.propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) o << "[Device Local]";
		if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) o << "[Host Visible]";
		if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) o << "[Host Coherent]";
		if (type.propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) o << "[Host Cached]";
		if (type.propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) o << "[Lazily Allocated]";
	};

	auto presentHeap = [](std::ostream& o, const VkMemoryHeap& heap)
	{
		tabs t(1);
		o << t << "heap size=" << heap.size << "\n";
		o << t << "heap flags: ";
		if (heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) o << "[Device Local]";
		if (heap.flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT) o << "[Multi Instance]";
	};

	o << "memory type:\n";
	for (size_t i = 0; i < properties.memoryTypeCount; ++i)
	{
		o << "[" << i << "]:\n";
		presentType(o, properties.memoryTypes[i]);
		o << std::endl;
	}
	o << "memory heap:\n";
	for (size_t i = 0; i < properties.memoryHeapCount; ++i)
	{
		o << "[" << i << "]:\n";
		presentHeap(o, properties.memoryHeaps[i]);
		o << std::endl;
	}
	return o;
}