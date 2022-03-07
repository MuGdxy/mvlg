#include <iostream>
#include <optional>
#include <array>
#include "utils.hpp"
#include <mvlg/mvlg.h>
#include <memory>
//#include "../MuVk/MuVk.h"
class ComputeShaderExample
{
	//std::array<float, 1024> inputData;
	//std::array<float, 1024> outputData;
	//constexpr VkDeviceSize inputDataSize() { return sizeof(inputData); }
	uint32_t computeShaderProcessUnit;
	const std::vector<const char*> validationLayers =
	{
		"VK_LAYER_KHRONOS_validation",
	};
public:
	ComputeShaderExample()
	{
		//inputData.fill(1.0f);
		//outputData.fill(0.0f);
	}

	vk::Instance instance;
	vk::DebugUtilsMessengerEXT debugMessenger;
	void createInstance()
	{
		vk::ApplicationInfo appInfo(
			"MuPipeGen Test", VK_MAKE_VERSION(1, 0, 0),
			"No Engine", VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_0);

		const std::vector validationLayers =
		{
			"VK_LAYER_KHRONOS_validation"
		};
		if (!vk::su::checkValidationLayerSupport(validationLayers))
			throw std::runtime_error("validation layers requested, but not available!");

		//get extension properties
		//auto extensionProperties = MuVk::Query::instanceExtensionProperties();
		//std::cout << extensionProperties << std::endl;

		//required extension
		const std::vector extensions =
		{
			VK_EXT_DEBUG_UTILS_EXTENSION_NAME
		};

		vk::InstanceCreateInfo instanceCreateInfo({}, &appInfo, validationLayers, extensions);
		vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo = vk::su::makeDebugUtilsMessengerCreateInfoEXT();
		//to debug instance
		instanceCreateInfo.pNext = &debugCreateInfo;
		instance = vk::createInstance(instanceCreateInfo);
		debugMessenger = instance.createDebugUtilsMessengerEXT(debugCreateInfo);
	}

	vk::PhysicalDevice physicalDevice;

	std::optional<uint32_t> queueFamilyIndex;

	void pickPhyscialDevice()
	{
		auto physicalDevices = instance.enumeratePhysicalDevices();
		for (const auto device : physicalDevices)
		{
			auto queueFamilies = device.getQueueFamilyProperties();
			for (size_t i = 0; i < queueFamilies.size(); ++i)
			{
				if (queueFamilies[i].queueFlags & (vk::QueueFlagBits::eCompute))
				{
					queueFamilyIndex = i;
					physicalDevice = device;
					break;
				}
			}
			if (queueFamilyIndex.has_value()) break;
		}
		if (!queueFamilyIndex.has_value())
			throw std::runtime_error("can't find a family that contains compute queue!");
		else
		{
			std::cout << "Select Physical Device:" << physicalDevice << std::endl;
			std::cout << "Select Queue Index:" << queueFamilyIndex.value() << std::endl;
		}
		auto p = physicalDevice.getProperties();
		std::cout << "maxComputeWorkGroupInvocations:" << p.limits.maxComputeWorkGroupInvocations << std::endl;
		computeShaderProcessUnit = p.limits.maxComputeWorkGroupInvocations < uint32_t(256) ?
			p.limits.maxComputeWorkGroupInvocations : uint32_t(256);
	}

	vk::Device device;
	vk::Queue queue;
	void createLogicalDevice()
	{
		auto priority = { 1.0f };//default
		vk::DeviceQueueCreateInfo queueCreateInfo({}, queueFamilyIndex.value(), priority);
		vk::DeviceCreateInfo createInfo({}, queueCreateInfo, {}, {}, nullptr);
		device = physicalDevice.createDevice(createInfo);
		queue = device.getQueue(queueFamilyIndex.value(), 0);
	}

	vk::Buffer storageBuffer;
	vk::DeviceMemory storageBufferMemory;
	void createStorageBuffer()
	{
		auto size = descriptorBlock->TotalSize();
		vk::BufferCreateInfo createInfo({}, size,
			descriptorBlock->BufferUsage(), vk::SharingMode::eExclusive);
		storageBuffer = device.createBuffer(createInfo);

		vk::MemoryRequirements requirements = device.getBufferMemoryRequirements(storageBuffer);

		vk::MemoryAllocateInfo allocInfo(requirements.size,
			vk::su::findMemoryType(physicalDevice.getMemoryProperties(), requirements.memoryTypeBits,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
		storageBufferMemory = device.allocateMemory(allocInfo);

		device.bindBufferMemory(storageBuffer, storageBufferMemory, 0);
	}

	vk::DescriptorSetLayout descriptorSetLayout;

	std::vector<uint8_t> readFile(const std::string& filename)
	{
		//at end
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			throw std::runtime_error("failed to open file!");
		}
		size_t fileSize = (size_t)file.tellg();
		std::vector<uint8_t> buffer(fileSize);
		file.seekg(0);
		file.read(reinterpret_cast<char*>(buffer.data()), fileSize);
		file.close();
		return buffer;
	}

	vk::PipelineLayout pipelineLayout;
	vk::Pipeline computePipeline;
	mvlg::LayoutGenerator layoutGenerator;
	std::shared_ptr<spv_reflect::ShaderModule> module;
	mvlg::SemanticConstants semanticConstants;
	mvlg::SemanticDescriptors semanticDescriptors;
	mvlg::SemanticDescriptors::Entry<uint32_t>* descriptorIndex;
	mvlg::SemanticDescriptors::Entry<std::vector<float>>* descriptorBlockData;
	mvlg::SemanticDescriptors::Entry<mvlg::NoStorage>* descriptorBlock;
	void createComputePipeline()
	{
		module = std::make_shared<spv_reflect::ShaderModule>(readFile(MU_SHADER_PATH "index.comp.spv"));

		mvlg::SpecializationConstants specializationConstants;
		{
			specializationConstants
				.AddConstant(0, computeShaderProcessUnit)
				.AddConstant(1, 1)
				.AddConstant(2, 2.0f);
		}
		layoutGenerator.Generate(device, { module }, specializationConstants);

		descriptorSetLayout = layoutGenerator.descriptorSetLayoutMap[0];
		pipelineLayout = layoutGenerator.pipelineLayout;
		semanticConstants.Init(module, pipelineLayout);
		semanticDescriptors.Init(module);
		descriptorIndex = semanticDescriptors.GetEntry<uint32_t>("block.index");
		descriptorBlockData = semanticDescriptors.GetEntry<std::vector<float>>("block.data");
		descriptorBlock = semanticDescriptors.GetEntry<mvlg::NoStorage>("block");
		descriptorIndex->data = 2;
		descriptorBlockData->data.resize(1024, 0.0f);
		descriptorBlockData->SetVariantArraySize(descriptorBlockData->data.size());
		descriptorBlock->SetVariantArraySize(descriptorBlockData->data.size());

		vk::ComputePipelineCreateInfo createInfo({},
			layoutGenerator.shaderStageCreateInfos.front(),
			layoutGenerator.pipelineLayout);

		vk::Result result;
		std::tie(result, computePipeline) = device.createComputePipeline(nullptr, createInfo);
		layoutGenerator.DestroyShaderModules();
	}

	void writeMemoryFromHost()
	{
		void* data = device.mapMemory(storageBufferMemory, 0, descriptorBlock->TotalSize());
		descriptorIndex->WriteMemory(data);
		descriptorBlockData->WriteMemory(data);
		//memcpy(data, inputData.data(), inputDataSize());
		device.unmapMemory(storageBufferMemory);
	}

	vk::DescriptorPool descriptorPool;
	void createDescriptorPool()
	{
		std::vector<vk::DescriptorPoolSize> poolSizes;
		for (auto& type : layoutGenerator.descriptorTypeStatics)
			poolSizes.emplace_back(type.first, type.second);
		vk::DescriptorPoolCreateInfo createInfo({},
			layoutGenerator.descriptorSetLayouts.size(), poolSizes);
		descriptorPool = device.createDescriptorPool(createInfo);
	}

	vk::DescriptorSet descriptorSet;
	void createDescriptorSet()
	{
		vk::DescriptorSetAllocateInfo allocInfo(descriptorPool, descriptorSetLayout);
		descriptorSet = device.allocateDescriptorSets(allocInfo).front();

		vk::DescriptorBufferInfo bufferInfo(storageBuffer, 0, descriptorBlock->TotalSize());
		auto type = descriptorBlock->DescriptorType();
		//vk::WriteDescriptorSet write(
		//	descriptorSet, descriptorBlock->Binding(), descriptorBlock->ArrayElement(),
		//	type, 
		//	{}, 
		//	bufferInfo, 
		//	{});

		auto writes =
		{
			descriptorBlock->WriteDescriptorSet(descriptorSet,bufferInfo)
		};

		device.updateDescriptorSets(writes, {});
	}

	vk::CommandPool commandPool;
	void createCommandPool()
	{
		vk::CommandPoolCreateInfo createInfo({}, queueFamilyIndex.value());
		commandPool = device.createCommandPool(createInfo);
	}

	vk::CommandBuffer commandBuffer;
	void execute()
	{
		std::cout << "input data:\n";
		for (size_t i = 0; i < descriptorBlockData->data.size(); ++i)
		{
			if (i % 64 == 0 && i != 0) std::cout << '\n';
			std::cout << descriptorBlockData->data[i];
		}
		std::cout << "\n";

		vk::CommandBufferAllocateInfo allocInfo(commandPool, vk::CommandBufferLevel::ePrimary, 1);
		commandBuffer = device.allocateCommandBuffers(allocInfo).front();
		vk::CommandBufferBeginInfo beginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
		commandBuffer.begin(beginInfo);

		auto entryA =
			semanticConstants.GetEntry<uint32_t>("PushConstant.a");
		auto entryFArray =
			semanticConstants.GetEntry<float[4]>("PushConstant.fArray[1]");
		entryA->data = 2;
		entryFArray->data[0] = 8.0f;
		entryFArray->data[1] = 8.0f;
		entryFArray->data[2] = 0.0f;

		entryA->PushConstant(commandBuffer);
		entryFArray->PushConstant(commandBuffer);

		commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, computePipeline);
		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout,
			0, descriptorSet, {});
		commandBuffer.dispatch(
			descriptorBlockData->DataSize() / computeShaderProcessUnit, //x
			1, //y
			1 //z
		);
		commandBuffer.end();
		vk::SubmitInfo submitInfo({}, {}, commandBuffer, {});
		queue.submit(submitInfo);
		queue.waitIdle();
		auto data = device.mapMemory(storageBufferMemory, 0, descriptorBlockData->DataSize());
		descriptorIndex->ReadMemory(data);
		descriptorBlockData->ReadMemory(data);
		device.unmapMemory(storageBufferMemory);

		std::cout << "output data:\n";
		for (size_t i = 0; i < descriptorBlockData->data.size(); ++i)
		{
			if (i % 64 == 0 && i != 0) std::cout << '\n';
			std::cout << descriptorBlockData->data[i];
		}
		std::cout << '\n';

		std::cout << "descriptorEntry:" << descriptorIndex->data << std::endl;
	}

	void Run()
	{
		createInstance();
		pickPhyscialDevice();
		createLogicalDevice();

		createComputePipeline();

		createStorageBuffer();
		writeMemoryFromHost();


		createDescriptorPool();
		createDescriptorSet();
		createCommandPool();

		execute();

		cleanUp();
	}

	void cleanUp()
	{
		device.destroyCommandPool(commandPool);
		device.destroyDescriptorPool(descriptorPool);
		device.destroyPipeline(computePipeline);

		layoutGenerator.DestroyDescriptorSetLayouts();
		layoutGenerator.DestroyPipelineLayout();

		device.destroyBuffer(storageBuffer);
		device.freeMemory(storageBufferMemory);
		instance.destroyDebugUtilsMessengerEXT(debugMessenger);
		device.destroy();
		instance.destroy();
	}
};

int main()
{
	ComputeShaderExample program;
	program.Run();
	//try
	//{
	//	program.Run();
	//}
	//catch (std::runtime_error e)
	//{
	//	std::cerr << e.what() << std::endl;
	//	return EXIT_FAILURE;
	//}
}
