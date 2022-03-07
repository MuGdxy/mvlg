# Mu Vulkan Layout Generator(mvlg)

C++17 vulkan automatic layout generator based on **vulkanhpp**, **SPIRV-Reflect** and my front-end compile generator **MuCplGen**.

mvlg can do:

- Generate Pipeline Layout from SPIRV

- Generate DescriptorSetLayout from SPIRV

- Generate Push Constant Layout from SPIRV

- Provide Push Constant Layout Entry by semantics like OpenGL, but more flexible

  ```cpp
  auto entryA = semanticConstants.GetEntry<uint32_t>("PushConstant.a");
  entryA->data = 2;
  ```

- Provide Descriptor Layout Entry by semantics

  ```glsl
  //glsl file
  #version 450
  layout(set = 0, binding = 0, std430) buffer StorageBuffer
  {
      int index;
      float data[];
  } block[2];
  ```

  ```cpp
  //.cpp file
  auto block = semanticDescriptors.GetEntry<mvlg::NoStorage>("block[0]");
  auto count = semanticDescriptors.GetEntry<uint32_t>("block[0].count");
  auto data = semanticDescriptors.GetEntry<std::vector<float>>("block[0].data");
  
  //set up data
  data->data.resize(1024, 0.0f);
  auto arraySize = descriptorBlockData->data.size();
  count->data = arraySize;
  data->SetVariantArraySize(arraySize);
  block->SetVariantArraySize(arraySize);//to get block total size for memory copy
  
  //memory copy
  void* dst = device.mapMemory(storageBufferMemory, 0, block->TotalSize());
  count->WriteMemory(dst);
  block->WriteMemory(dst);
  
  //write descriptor set
  vk::DescriptorBufferInfo bufferInfo(storageBuffer, 0, descriptorBlock->TotalSize());
  auto write =  block->WriteDescriptorSet(descriptorSet, bufferInfo);
  device.updateDescriptorSets(writes, {});
  ```



## Build

```bash
git clone https://github.com/MuGdxy/mvlg.git
cd ./mvlg
git submodule update --init
```

cmake:

Turn On `MVLG_EXAMPLES` to build examples.

Turn On `MVLG_INCLUDE_VULKAN_HPP`  to automatically include **Vulkan-Hpp**. ELSE, include **Vulkan-Headers** and **Vulkan-Hpp** manully by yourself.

## Quick Look

If we have such a shader.

```glsl
//file: example.comp
#version 450
layout (local_size_x_id = 0) in;
layout (push_constant, std430) uniform PushConstant
{
    int a;
    float[2][4] fArray;
};
void main()
{
	...
}
```

in cpp we can do like this:

```cpp
mvlg::LayoutGenerator layoutGenerator;
vk::PipelineLayout pipelineLayout;
vk::Pipeline pipeline;
mvlg::SemanticConstants semanticConstants;
void CreatePipelineLayout()
{
    auto module = std::make_shared<spv_reflect::ShaderModule>(readFile("example.comp.spv"));
    //Specialization Constants
    mvlg::SpecializationConstants specializationConstants;
    specializationConstants.AddConstant(0,256);//<example.comp>: layout (local_size_x_id = 0) in;
    
    //Layout Generator
	layoutGenerator.Generate(device, { module }, specializationConstants);
	//Get Pipeline Layout
    pipelineLayout = layoutGenerator.pipelineLayout;
    //Set up Semantic Constants(for push constants)
    semanticConstants.Init(module, pipelineLayout);
    
    //Create Pipeline
    vk::ComputePipelineCreateInfo createInfo({},
			layoutGenerator.shaderStageCreateInfos.front(),
			layoutGenerator.pipelineLayout);
	vk::Result result;
	std::tie(result, computePipeline) = device.createComputePipeline(nullptr, createInfo);
    //Destroy Shader Modules
	layoutGenerator.DestroyShaderModules();
}

void PushConstants(vk::CommandBuffer commandBuffer)
{
    //OpenGL-like Query
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
}

void CleanUp()
{
    layoutGenerator.DestroyDescriptorSetLayouts();
    layoutGenerator.DestroyPipelineLayout();
}
```

