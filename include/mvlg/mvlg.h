#include <spirv_reflect.h>
#include <list>
#include <unordered_map>
#include <vector>
#include <functional>
#include <vulkan/vulkan.hpp>
#include <memory>
#include <MuCplGen/MuCplGen.h>
#include <MuCplGen/SubModules/Sequence.h>

namespace mvlg
{
	class SpecializationConstants
	{
		using Ty = uint32_t;
		std::vector<Ty> data;
		std::vector<vk::SpecializationMapEntry> entries;
		void AddConstant(uint32_t constantId, void* v, uint32_t size)
		{
			for (size_t i = 0; i < size/sizeof(Ty); ++i)
				data.push_back(reinterpret_cast<Ty*>(v)[i]);
			entries.emplace_back(constantId, data.size() * sizeof(Ty) - size, size);
		}
	public:
#define Constant(Ty) SpecializationConstants& AddConstant(uint32_t constantId, Ty v) { AddConstant(constantId, &v, sizeof(Ty)); return *this; }
		Constant(int32_t)
		Constant(uint32_t)
		Constant(float)
		Constant(double)
#undef Constant
		vk::SpecializationInfo GetSpecializationInfo() const
		{
			return vk::SpecializationInfo(entries.size(), entries.data(),
				data.size() * sizeof(Ty), data.data());
		}
	};

	struct PushConstantRangeCounter
	{
		std::vector<vk::PushConstantRange> ranges;
		vk::ShaderStageFlags stageFlags = vk::ShaderStageFlagBits::eAll;
		void SetStageFlags(SpvReflectShaderStageFlagBits flags)
		{
			stageFlags = (vk::ShaderStageFlags)flags;
		}
		void AddPushConstantRange(const SpvReflectBlockVariable* variable);
	};

	struct DescriptorSetLayoutCounter
	{
		std::unordered_map<uint32_t, std::vector<vk::DescriptorSetLayoutBinding>> setBindingsMap;
		std::vector<vk::DescriptorSetLayoutCreateInfo> layoutCreateInfos;
		std::vector<uint32_t> sets;
		vk::ShaderStageFlags stageFlags = vk::ShaderStageFlagBits::eAll;

		void SetStageFlags(SpvReflectShaderStageFlagBits flags)
		{
			stageFlags = (vk::ShaderStageFlags)flags;
		}

		void AddDescriptorSetLayout(const SpvReflectDescriptorSet* set, 
			std::function<uint32_t(const SpvReflectDescriptorBinding&)> setVariantCount);

		void CreateDescriptorSetLayouts(const vk::Device& device,
			std::vector<vk::DescriptorSetLayout>& descriptorSetLayouts,
			std::unordered_map<uint32_t, vk::DescriptorSetLayout>& descriptorSetLayoutMap,
			std::unordered_map<vk::DescriptorType, uint32_t>& descriptorTypeStatics);
	};

	class LayoutGenerator
	{
		vk::Device device;
		std::vector<vk::SpecializationInfo> specializationInfos;
		std::vector<SpecializationConstants> specializationConstants;
		PushConstantRangeCounter pushConstantCounter;
		DescriptorSetLayoutCounter descriptorSetLayoutCounter;
		std::function<uint32_t(const SpvReflectDescriptorBinding&)> setVariantCount;
	public:
		LayoutGenerator() { }
		
		void SetVariantDescriptorCount(std::function<uint32_t(const SpvReflectDescriptorBinding&)> setVariantCount)
		{
			this->setVariantCount = setVariantCount;
		}

		void Generate(const vk::Device& device,
			const std::vector<std::shared_ptr<spv_reflect::ShaderModule>>& modules,
			const SpecializationConstants& specializationConstants);

		void Generate(const vk::Device& device,
			const std::vector<std::shared_ptr<spv_reflect::ShaderModule>>& modules,
			const std::vector<uint32_t>& specializationConstantsIndex = {},
			const std::vector<SpecializationConstants>& specializationConstants = {});

		std::vector<vk::DescriptorSetLayout> descriptorSetLayouts;
		std::unordered_map<uint32_t, vk::DescriptorSetLayout> descriptorSetLayoutMap;
		std::unordered_map<vk::DescriptorType, uint32_t> descriptorTypeStatics;
		std::vector<vk::PipelineShaderStageCreateInfo> shaderStageCreateInfos;
		vk::PipelineLayout pipelineLayout;
		std::vector<vk::ShaderModule> shaderModules;
		~LayoutGenerator();
		void DestroyPipelineLayout() { device.destroyPipelineLayout(pipelineLayout); }
		void DestroyDescriptorSetLayouts()
		{
			for(auto layout : descriptorSetLayouts)
				device.destroyDescriptorSetLayout(layout);
		}
		void DestroyShaderModules()
		{
			for (auto module : shaderModules)
				device.destroyShaderModule(module);
		}
	};

	class EntryScanner : public MuCplGen::Scanner<MuCplGen::EasyToken>
	{
		using Token = MuCplGen::EasyToken;
	public:
		EntryScanner();
	};

	class EntryParser :public MuCplGen::SyntaxDirected<MuCplGen::SLRParser<MuCplGen::EasyToken>>
	{
		MuCplGen::Sequence<uint32_t> sq;
		constexpr int ScalarSize() { return 4; }
		struct
		{
			const std::vector<SpvReflectBlockVariable*>* heads = nullptr;
			SpvReflectBlockVariable* current = nullptr;
		} pushConstant;

		struct
		{
			const std::vector<SpvReflectDescriptorSet*>* sets = nullptr;
			SpvReflectDescriptorBinding* binding = nullptr;
			SpvReflectBlockVariable* current = nullptr;
		} descriptor;

		bool inArray = false;
		enum class EntryType
		{
			PushConstant,
			Descriptor
		} type;
	public:
		EntryParser(std::ostream& log = std::cout);
		void SetUp(const std::vector<SpvReflectBlockVariable*>* blocks)
		{
			pushConstant.heads = blocks;
			pushConstant.current = nullptr;
			type = EntryType::PushConstant;
			CommonReset();
		}		
		void SetUp(const std::vector<SpvReflectDescriptorSet*>* sets)
		{
			descriptor.sets = sets;
			descriptor.binding = nullptr;
			descriptor.current = nullptr;
			isArray = false;
			type = EntryType::Descriptor;
			CommonReset();
		}
		void CommonReset()
		{
			inArray = false;
			offset = 0;
			size = 0;
			set = 0;
			binding = 0;
			bindingArrayIndex = 0;
		}
		uint32_t offset;
		uint32_t size;

		uint32_t set;
		uint32_t binding;
		vk::DescriptorType descriptorType;
		bool isArray = false;
		uint32_t descriptorArraySize;

		bool hasVariantArray;
		uint32_t bindingArrayIndex;
		uint32_t variantArrayStartOffset;
		uint32_t variantArrayStride;
	};

	struct DescriptorInfo
	{
		uint32_t offset;
		uint32_t size;
		uint32_t set;
		uint32_t binding;
		uint32_t bindingArrayIndex;
		vk::DescriptorType descriptorType;
		bool hasVariantArray;
		uint32_t variantArrayStartOffset;
		uint32_t variantArrayStride;
		
		bool isArray;
		uint32_t descriptorArraySize;
	};

	class EntryGen
	{
		EntryScanner scanner;
		EntryParser parser;
	public:
		EntryGen(std::ostream& log = std::cout)
			:parser(log){}
		bool GetEntryOffsetSize(
			const std::vector<SpvReflectBlockVariable*>& blocks,
			const std::string& semantic, uint32_t& offset, uint32_t& size)
		{
			std::vector<MuCplGen::LineContent> lc = {
				{semantic, 0}
			};
			auto tokens = scanner.Scann(lc);
			parser.SetUp(&blocks);
			bool res = parser.Parse(lc, tokens);
			offset = parser.offset;
			size = parser.size;
			return res;
		}

		bool GetEntryDescriptorInfo(
			const std::vector<SpvReflectDescriptorSet*>& sets,
			const std::string& semantic, DescriptorInfo& info
		);
	};

	extern EntryGen entryGen;

	class SemanticConstants
	{
		vk::PipelineLayout layout;
		vk::ShaderStageFlags stage;
		class BaseEntry
		{
			friend class SemanticConstants;
		protected:
			SemanticConstants* owner;
			std::string semantic;
			uint32_t offset;
			uint32_t size;
			virtual void* Value() = 0;
		public:
			const std::string& Name() { return semantic; }
			virtual ~BaseEntry() {};
			void PushConstant(vk::CommandBuffer& cmd);
		};
		std::vector<std::shared_ptr<spv_reflect::ShaderModule>> shaderModules;
		std::list<std::unique_ptr<BaseEntry>> entries;
		std::vector<SpvReflectBlockVariable*> heads;
		void GetHeads(const std::shared_ptr<spv_reflect::ShaderModule>& module);

	public:
		template<typename Ty>
		class Entry : public BaseEntry
		{
		protected:
			virtual void* Value() override { return &data; }
		public:
			Ty data;
			virtual ~Entry() override {};
		};

		template<typename Ty, int N>
		class Entry<std::array<Ty, N>> : public BaseEntry
		{
		protected:
			virtual void* Value() override { return data.data(); }
		public:
			std::array<Ty, N> data;
			virtual ~Entry() override {};
		};

		void Init(
			std::shared_ptr<spv_reflect::ShaderModule> module,
			const vk::PipelineLayout& layout);

		void Init(
			const std::vector< std::shared_ptr<spv_reflect::ShaderModule>>& modules,
			const vk::PipelineLayout& layout);

		template<typename Ty>
		Entry<Ty>* GetEntry(const std::string& semantic);
	};
	
	struct NoStorage {};

	class SemanticDescriptors
	{
		//vk::PipelineLayout layout;
		//vk::ShaderStageFlags stage;
		class BaseEntry
		{
			friend class SemanticDescriptors;
		protected:
			std::string semantic;
			DescriptorInfo descriptorInfo;
			virtual void* Value() = 0;
			bool Check()
			{
				if (descriptorInfo.size == 0)
					throw std::runtime_error(
						"this entry has variant array, but never set array size,"
						"call SetVariantArraySize() to set array size");
				if (!Value()) throw std::runtime_error("this entry is <NoStorage>, doesn't allow memory copy");
			}
		public:
			const std::string& Name() { return semantic; }
			virtual ~BaseEntry() {};
			void WriteMemory(void* descriptorMemoryStart)
			{ 
				Check();
				memcpy(
					(char*)descriptorMemoryStart + descriptorInfo.offset,
					Value(), descriptorInfo.size); 
			}
			void ReadMemory(void* descriptorMemoryStart)
			{ 
				Check();
				memcpy(
					Value(), (char*)descriptorMemoryStart + descriptorInfo.offset,
				descriptorInfo.size); 
			}
			bool HasVariantArray() { return descriptorInfo.hasVariantArray; }
			uint32_t VariantArrayOffset() { return descriptorInfo.variantArrayStartOffset; }
			void SetVariantArraySize(uint32_t size)
			{
				if (!descriptorInfo.hasVariantArray)
					throw std::runtime_error("this descriptor doesn't allow variant array");
				descriptorInfo.size = 
					descriptorInfo.variantArrayStartOffset - descriptorInfo.offset 
					+ size * descriptorInfo.variantArrayStride;
			}
			uint32_t TotalSize() {return descriptorInfo.size;}
			vk::DescriptorType DescriptorType() { return descriptorInfo.descriptorType; }
			vk::BufferUsageFlags BufferUsage();
			uint32_t Binding() { return descriptorInfo.binding; }
			uint32_t Set() { return descriptorInfo.set; }
			uint32_t ArrayElement() { return descriptorInfo.bindingArrayIndex; }
			uint32_t IsDescriptorArray() { return descriptorInfo.isArray; }
			uint32_t DescriptorArraySize() { return descriptorInfo.descriptorArraySize; }
#define WriteDptorSet(def,x,y,z) vk::WriteDescriptorSet WriteDescriptorSet(vk::DescriptorSet set, def)\
			{\
				return vk::WriteDescriptorSet(set, Binding(), ArrayElement(), DescriptorType(),\
					x, y, z);\
			}

			WriteDptorSet(vk::DescriptorImageInfo imageInfo, imageInfo, {}, {});
			WriteDptorSet(vk::DescriptorBufferInfo bufferInfo, {}, bufferInfo,{});
			WriteDptorSet(vk::BufferView texelBufferView, {}, {}, texelBufferView);
#undef WriteDptorSet
		};

		std::vector<std::shared_ptr<spv_reflect::ShaderModule>> shaderModules;
		std::list<std::unique_ptr<BaseEntry>> entries;
		std::vector<SpvReflectDescriptorSet*> sets;
		void GetSets(const std::shared_ptr<spv_reflect::ShaderModule>& module);
	public:
		template<typename Ty>
		class Entry : public BaseEntry
		{
		protected:
			virtual void* Value() override { return &data; }
		public:
			Ty data;
			virtual ~Entry() override {};
		};

		template<>
		class Entry<NoStorage> : public BaseEntry
		{
		protected:
			virtual void* Value() override { return nullptr; }
		public:
			virtual ~Entry() override {};
		};

		template<typename Ty, int N>
		class Entry<std::array<Ty, N>> : public BaseEntry
		{
		protected:
			virtual void* Value() override { return data.data(); }
		public:
			std::array<Ty, N> data;
			constexpr uint32_t DataSize() { return data.size() * sizeof(Ty); }
			virtual ~Entry() override {};
		};

		template<typename Ty>
		class Entry<std::vector<Ty>> : public BaseEntry
		{
		protected:
			virtual void* Value() override { return data.data(); }
		public:
			std::vector<Ty> data;
			virtual ~Entry() override {};
			uint32_t DataSize() { return data.size() * sizeof(Ty); }
		};

		void Init(std::shared_ptr<spv_reflect::ShaderModule> module)
		{
			shaderModules.push_back(module);
		}

		void Init(const std::vector<std::shared_ptr<spv_reflect::ShaderModule>>& modules)
		{
			shaderModules = modules;
		}
		template<typename Ty>
		Entry<Ty>* GetEntry(const std::string& semantic);
	};

	template<typename Ty>
	SemanticDescriptors::Entry<Ty>* SemanticDescriptors::GetEntry(const std::string& semantic)
	{
		auto entry = std::make_unique<Entry<Ty>>();
		for (auto& module : shaderModules)
		{
			GetSets(module);
			if (entryGen.GetEntryDescriptorInfo(sets, semantic, entry->descriptorInfo))
			{
				auto ptr = entry.get();
				entry->semantic = semantic;
				entries.push_back(std::move(entry));
				if (ptr->descriptorInfo.isArray && !std::is_same_v<Ty, NoStorage>)
				{
					throw std::runtime_error("this entry points to a descriptor array, "
						"shouldn't have a certain storage. please use Entry<NoStorage>");
				}
				return ptr;
			}
		}
		return nullptr;
	}

	template<typename Ty>
	SemanticConstants::Entry<Ty>* SemanticConstants::GetEntry(const std::string& semantic)
	{
		auto entry = std::make_unique<Entry<Ty>>();
		for (auto& module : shaderModules)
		{
			GetHeads(module);
			if (entryGen.GetEntryOffsetSize(heads, semantic, entry->offset, entry->size))
			{
				entry->owner = this;
				entry->semantic = semantic;
				auto ptr = entry.get();
				entries.push_back(std::move(entry));
				return ptr;
			}
		}
		return nullptr;
	}

	//template<typename Ty>
	//SemanticConstants::Entry<Ty>::~Entry(){}
}