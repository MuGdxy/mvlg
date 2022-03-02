#include <mvlg/mvlg.h>
#include <iostream>
#include <sstream>
#define MU_CHECK(x, info) if((x)!=0) throw std::runtime_error((info));
namespace mvlg
{
	using namespace vk;
	struct PushConstantRangeCounter
	{
		std::vector<PushConstantRange> ranges;
		ShaderStageFlags stageFlags = ShaderStageFlagBits::eAll;
		void SetStageFlags(SpvReflectShaderStageFlagBits flags)
		{
			stageFlags = (ShaderStageFlags)flags;
		}
		void AddPushConstantRange(const SpvReflectBlockVariable* variable);
	};

	struct DescriptorSetLayoutCounter
	{
		std::vector<DescriptorSetLayoutCreateInfo> layouts;
		std::vector<DescriptorSetLayoutBinding> bindings;
		std::vector<uint32_t> sets;
		ShaderStageFlags stageFlags = ShaderStageFlagBits::eAll;
		void SetStageFlags(SpvReflectShaderStageFlagBits flags)
		{
			stageFlags = (ShaderStageFlags)flags;
		}
		void AddDescriptorSetLayout(const SpvReflectDescriptorSet* set)
		{
			auto start = bindings.size();
			for (size_t i = 0; i < set->binding_count; ++i)
			{
				auto binding = set->bindings[i];
				//TODO: sampler
				//std::vector<Sampler> immutable samplers

				bindings.emplace_back(binding->binding,
					(DescriptorType)binding->descriptor_type, binding->count, stageFlags, nullptr);
			}
			auto count = set->binding_count;
			layouts.emplace_back((DescriptorSetLayoutCreateFlags)0, count, &bindings[start]);
			sets.emplace_back(set->set);
		}

		void CreateDescriptorSetLayouts(const vk::Device& device,
			std::vector<DescriptorSetLayout>& descriptorSetLayouts,
			std::unordered_map<uint32_t, DescriptorSetLayout>& descriptorSetLayoutMap,
			std::unordered_map<vk::DescriptorType, uint32_t>& descriptorTypeStatics)
		{
			descriptorSetLayouts.reserve(layouts.size());
			for (size_t i = 0; i < layouts.size(); ++i)
			{
				auto descriptorLayout = device.createDescriptorSetLayout(layouts[i]);
				descriptorSetLayouts.emplace_back(descriptorLayout);
				descriptorSetLayoutMap.emplace(sets[i], descriptorLayout);
			}
			for (auto binding : bindings)
			{
				auto iter = descriptorTypeStatics.find(binding.descriptorType);
				if (iter == descriptorTypeStatics.end())
					descriptorTypeStatics.insert({ binding.descriptorType, binding.descriptorCount });
				else iter->second += binding.descriptorCount;
			}
		}
	};

	void PushConstantRangeCounter::AddPushConstantRange(const SpvReflectBlockVariable* variable)
	{
		ranges.emplace_back(stageFlags, variable->absolute_offset, variable->size);
		//for (size_t i = 0; i < variable->member_count; ++i)
		//	AddPushConstantRange(&variable->members[i]);
	}

	LayoutGenerator::LayoutGenerator(const vk::Device& device,
		const std::vector<std::shared_ptr<spv_reflect::ShaderModule>>& modules,
		const SpecializationInfo* specializationInfo)
	{
		Generate(device, modules, specializationInfo);
	}

	LayoutGenerator::LayoutGenerator(const vk::Device& device,
		const std::vector<std::shared_ptr<spv_reflect::ShaderModule>>& modules,
		const SpecializationConstants& specializationConstants)
	{
		Generate(device, modules, specializationConstants);
	}

	void LayoutGenerator::Generate(const vk::Device& device,
		const std::vector<std::shared_ptr<spv_reflect::ShaderModule>>& modules,
		const SpecializationInfo* specializationInfo)
	{
		if (this->device) throw std::runtime_error("LayoutGenerator should be setup only once");
		this->device = &device;
		PushConstantRangeCounter pushConstantCounter;
		DescriptorSetLayoutCounter descriptorSetLayoutCounter;
		shaderStageCreateInfos.reserve(modules.size());
		shaderModules.reserve(modules.size());
		for (auto module : modules)
		{
			ShaderModuleCreateInfo shaderModuleCreateInfo({}, module->GetCodeSize(), module->GetCode());
			shaderModules.push_back(device.createShaderModule(shaderModuleCreateInfo));
			auto stage = module->GetShaderStage();
			shaderStageCreateInfos.emplace_back(
				PipelineShaderStageCreateFlags(0),
				(ShaderStageFlagBits)stage, shaderModules.back(),
				module->GetEntryPointName(), specializationInfo);

			uint32_t count;
			MU_CHECK(module->EnumeratePushConstantBlocks(&count, nullptr),
				"failed to get push constant block count");
			std::vector<SpvReflectBlockVariable*> pushConstants(count);
			MU_CHECK(module->EnumeratePushConstantBlocks(&count, pushConstants.data()),
				"failed to get push constant blocks");

			pushConstantCounter.SetStageFlags(stage);
			for (const auto v : pushConstants) pushConstantCounter.AddPushConstantRange(v);

			MU_CHECK(module->EnumerateDescriptorSets(&count, nullptr),
				"failed to get descriptor set count");
			std::vector<SpvReflectDescriptorSet*> descriptorSets(count);
			MU_CHECK(module->EnumerateDescriptorSets(&count, descriptorSets.data()),
				"failed to get descriptor sets");

			descriptorSetLayoutCounter.SetStageFlags(stage);
			for (const auto set : descriptorSets)
				descriptorSetLayoutCounter.AddDescriptorSetLayout(set);
		}

		descriptorSetLayoutCounter.CreateDescriptorSetLayouts(device,
			descriptorSetLayouts, descriptorSetLayoutMap, descriptorTypeStatics);
		PipelineLayoutCreateInfo createInfo({}, descriptorSetLayouts, pushConstantCounter.ranges);
		pipelineLayout = device.createPipelineLayout(createInfo);
	}

	void LayoutGenerator::Generate(const vk::Device& device,
		const std::vector<std::shared_ptr<spv_reflect::ShaderModule>>& modules,
		const SpecializationConstants& specializationConstants)
	{
		specializationInfo = specializationConstants.GetSpecializationInfo();
		Generate(device, modules, &specializationInfo);
	}

	//SemanticConstants::BaseEntry::~BaseEntry(){}

	void SemanticConstants::BaseEntry::PushConstant(vk::CommandBuffer& cmd)
	{
		cmd.pushConstants(owner->layout, owner->stage, offset, size, Value());
	}

	void SemanticConstants::GetHeads()
	{
		if (heads.empty())
		{
			uint32_t count;
			MU_CHECK(shaderModule->EnumeratePushConstantBlocks(&count, nullptr),
				"failed to get push constant block count");
			heads.resize(count);
			MU_CHECK(shaderModule->EnumeratePushConstantBlocks(&count, heads.data()),
				"failed to get push constant blocks");
		}
	}
	void SemanticConstants::Init(
		std::shared_ptr<spv_reflect::ShaderModule> module,
		const vk::PipelineLayout& layout)
	{
		shaderModule = module;
		this->layout = layout;
		stage = (vk::ShaderStageFlags)module->GetShaderStage();
	}

	void SemanticDescriptors::GetSets()
	{
		if (sets.empty())
		{
			uint32_t count;
			MU_CHECK(shaderModule->EnumerateDescriptorSets(&count, nullptr),
				"failed to get descriptor set count");
			sets.resize(count);
			MU_CHECK(shaderModule->EnumerateDescriptorSets(&count, sets.data()),
				"failed to get descriptor sets");
		}
	}

	bool EntryGen::GetEntryDescriptorInfo(const std::vector<SpvReflectDescriptorSet*>& sets, const std::string& semantic, DescriptorInfo& info)
	{
		std::vector<MuCplGen::LineContent> lc = {
			{semantic, 0}
		};
		auto tokens = scanner.Scann(lc);
		MuCplGen::Debug::Highlight(lc, tokens);
		parser.SetUp(&sets);
		bool res = parser.Parse(lc, tokens);
		if (res)
		{
			info.offset = parser.offset;
			info.size = parser.size;
			info.set = parser.set;
			info.binding = parser.binding;
			info.bindingArrayIndex = parser.bindingArrayIndex;
			info.descriptorType = parser.descriptorType;
			info.isArray = parser.isArray;
			info.descriptorArraySize = parser.descriptorArraySize;
			if (!info.size && !info.isArray)
			{
				info.hasVariantArray = true;
				info.variantArrayStartOffset = parser.variantArrayStartOffset;
				info.variantArrayStride = parser.variantArrayStride;
			}
			else
			{
				info.hasVariantArray = false;
			}
		}
		return res;
	}

	vk::BufferUsageFlags mvlg::SemanticDescriptors::BaseEntry::BufferUsage()
	{
		switch (descriptorInfo.descriptorType)
		{
		case vk::DescriptorType::eSampler:
		case vk::DescriptorType::eCombinedImageSampler:
		case vk::DescriptorType::eSampledImage:
		case vk::DescriptorType::eUniformBuffer:
		case vk::DescriptorType::eUniformBufferDynamic:
			return vk::BufferUsageFlagBits::eUniformBuffer;

		case vk::DescriptorType::eUniformTexelBuffer:
			return vk::BufferUsageFlagBits::eUniformTexelBuffer;

		case vk::DescriptorType::eStorageBuffer:
		case vk::DescriptorType::eStorageImage:
		case vk::DescriptorType::eStorageBufferDynamic:
			return vk::BufferUsageFlagBits::eStorageBuffer;

		case vk::DescriptorType::eStorageTexelBuffer:
			return vk::BufferUsageFlagBits::eStorageTexelBuffer;

		case vk::DescriptorType::eInputAttachment:
			//TODO: Input Variable Detection
			return vk::BufferUsageFlagBits::eVertexBuffer
				| vk::BufferUsageFlagBits::eIndexBuffer
				| vk::BufferUsageFlagBits::eIndirectBuffer;

		case vk::DescriptorType::eInlineUniformBlock:
		case vk::DescriptorType::eAccelerationStructureKHR:
		case vk::DescriptorType::eAccelerationStructureNV:
		case vk::DescriptorType::eMutableVALVE:
		default:
			throw std::runtime_error("not support");
			break;
		}
		throw std::runtime_error("miss matching");
	}

	LayoutGenerator::~LayoutGenerator() {}

	EntryScanner::EntryScanner()
	{
		using namespace MuCplGen;
		auto& num = CreateRule();
		num.tokenType = "number";
		num.expression = R"(^([1-9]\d*|0))";
		num.onSucceed = [this](std::smatch&, Token& token)
		{
			token.type = Token::TokenType::number;
			token.color = ConsoleForegroundColor::Yellow;
			return SaveToken;
		};

		auto& id = CreateRule();
		id.tokenType = "identifier";
		id.expression = R"(^([a-z]|[A-Z]|_)\w*)";
		id.onSucceed = [this](std::smatch&, Token& token)
		{
			token.type = Token::TokenType::identifier;
			token.color = ConsoleForegroundColor::White;
			return SaveToken;
		};

		auto& sep = CreateRule();
		sep.tokenType = "separator";
		sep.expression = R"(^(\.|\[|\]))";
		sep.onSucceed = [this](std::smatch&, Token& token)
		{
			token.type = Token::TokenType::separator;
			token.color = ConsoleForegroundColor::Gray;
			return SaveToken;
		};
	}

	EntryParser::EntryParser(std::ostream& log)
		: SyntaxDirected(log), sq(this, "Idx")
	{
		using namespace MuCplGen;
		debug_option = Debug::DebugOption::AllDebugInfo;
		generation_option = BuildOption::Runtime;

		{
			//translate number token as terminator "num"
			auto& t = CreateTerminator("num");
			t.translation = [this](const Token& token)
			{
				return token.type == Token::TokenType::number;
			};
		}

		{
			//translate number token as terminator "num"
			auto& t = CreateTerminator("member");
			t.translation = [this](const Token& token)
			{
				return token.type == Token::TokenType::identifier;
			};
		}


		{
			auto& p = CreateParseRule();
			p.expression = "Entry -> Member";
		}

		{
			auto& p = CreateParseRule();
			p.expression = "Mem -> member";
			p.SetAction(
				[this](Empty)
				{
					auto& token = CurrentToken();
					switch (type)
					{
					case EntryType::PushConstant:
					{
						auto& current = pushConstant.current;
						auto& heads = pushConstant.heads;
						if (inArray)
						{
							this->log << "error array indexing";
							throw SemanticError(ParserErrorCode::Stop);
						}
						if (!current)
						{
							for (auto h : *heads)
								if (token.name == h->name) current = h;
						}
						else
						{
							for (size_t i = 0; i < current->member_count; ++i)
								if (token.name == current->members[i].name)
									current = &current->members[i];
						}
						if (!current)
						{
							this->log << "can't find <" << token.name << ">\n";
							throw SemanticError(ParserErrorCode::Stop);
						}
						offset = current->absolute_offset;
						size = current->size;
					}
					break;
					case EntryType::Descriptor:
					{
						auto& sets = descriptor.sets;
						auto& binding = descriptor.binding;
						auto& current = descriptor.current;
						if (inArray)
						{
							this->log << "error array indexing";
							throw SemanticError(ParserErrorCode::Stop);
						}
						if (!binding)
						{
							for (auto s : *sets)
							{
								for (size_t i = 0; i < s->binding_count; ++i)
								{
									if (s->bindings[i]->name == token.name)
									{
										binding = s->bindings[i];
										this->binding = binding->binding;
										auto bindingArr = binding->type_description->traits.array;
										this->descriptorArraySize = bindingArr.dims_count == 0 ? 1 : bindingArr.dims[0];
										this->set = s->set;
										this->isArray = bindingArr.dims_count > 0;
										this->descriptorType = (vk::DescriptorType)binding->descriptor_type;
									}
								}
							}
							if (!binding)
							{
								this->log << "can't find binding <" << token.name << ">\n";
								throw SemanticError(ParserErrorCode::Stop);
							}
							offset = binding->block.absolute_offset;
							size = binding->block.size;
							if (size == 0)
							{
								hasVariantArray = true;
								auto b = &binding->block;
								if (b->member_count)
									b = &b->members[b->member_count - 1];
								variantArrayStartOffset = b->absolute_offset;
								const auto& arrayInfo = b->type_description->traits.array;
								auto stride = arrayInfo.stride;
								for (size_t i = 0; i < arrayInfo.dims_count; i++)
								{
									if (!arrayInfo.dims[i]) continue;
									stride *= arrayInfo.dims[i];
								}
								variantArrayStride = stride;
							}
						}
						else
						{
							if (!current)
							{
								for (size_t i = 0; i < binding->block.member_count; ++i)
									if (binding->block.members[i].name == token.name)
										current = &binding->block.members[i];
							}
							else
							{
								for (size_t i = 0; i < current->member_count; ++i)
									if (token.name == current->members[i].name)
										current = &current->members[i];
							}
							if (!current)
							{
								this->log << "can't find <" << token.name << ">\n";
								throw SemanticError(ParserErrorCode::Stop);
							}
							offset = current->absolute_offset;
							size = current->size;
							isArray = false;
						}
					}
					break;
					default:
						break;
					}
					return token.name;
				});
		}

		{
			auto& p = CreateParseRule();
			p.expression = "Member -> Mem";
			p.SetAction(PassOn(0));
		}

		{
			auto& p = CreateParseRule();
			p.expression = "Num -> num";
			p.SetAction(
				[this](Empty)
				{
					auto& token = CurrentToken();
					return static_cast<uint32_t>(std::stoul(token.name));
				});
		}

		sq.CreateRules("Cmps");
		{
			auto& p = CreateParseRule();
			p.expression = "Idx.Comp -> [ Num ]";
			p.SetAction(PassOn(1));
		}

		{
			auto& p = CreateParseRule();
			p.expression = "Array -> Mem Idx.Cmps";
			p.SetAction(
				[this](std::string& name, std::vector<uint32_t>& dims)
				{
					switch (type)
					{
					case EntryType::PushConstant:
					{
						auto& current = pushConstant.current;
						auto& heads = pushConstant.heads;
						auto& arr = current->type_description->traits.array;
						const auto total_dims = arr.dims_count;
						if (total_dims < dims.size())
						{
							this->log << "error array axes: <" << name << ">, your"
								<< dims.size() << "are more than aim'" << total_dims
								<< "\n";
							throw SemanticError(ParserErrorCode::Stop);
						};
						for (size_t i = 0; i < dims.size(); ++i)
						{
							if (arr.dims[i] <= dims[i])
							{
								this->log << "error array dimension-" << i << ": <" << name << ">, yours has "
									<< dims[i] << "however aim has" << arr.dims[i]
									<< "\n";
								throw SemanticError(ParserErrorCode::Stop);
							}
						}
						const auto stride = arr.stride;
						auto current_stride = stride;
						uint32_t local_offset = 0;
						for (size_t i = 0; i < dims.size(); ++i)
						{
							current_stride = stride;
							for (size_t j = i + 1; j < total_dims; ++j)
								current_stride *= arr.dims[j];
							local_offset += dims[i] * current_stride;
						}
						offset += local_offset;
						size = current_stride;
						if (dims.size() < total_dims) inArray = true;
					}
					break;
					case EntryType::Descriptor:
					{
						auto& sets = descriptor.sets;
						auto& binding = descriptor.binding;
						auto& current = descriptor.current;
						if (current == nullptr)//outside of buffer
						{
							if (binding->array.dims_count < dims.size())
							{
								this->log << "too large array axes: <" << name << ">, your "
									<< dims.size() << "are more than aim's " << binding->array.dims_count;
								if(dims.size() > 1)
									this->log << "Vulkan only supports single array level for this resource\n";
								throw SemanticError(ParserErrorCode::Stop);
							}
							if (binding->array.dims[0] <= dims[0])
							{
								this->log << "error array dimension-" << 0 << ": <" << name << ">, yours has "
									<< dims[0] << "however aim has" << current->array.dims[0]
									<< "\n";
								throw SemanticError(ParserErrorCode::Stop);
							}
							this->bindingArrayIndex = dims[0];
							this->descriptorType = (vk::DescriptorType)binding->descriptor_type;
							this->isArray = false;
						}
						else//inside of buffer
						{
							auto& arr = current->type_description->traits.array;
							const auto total_dims = arr.dims_count;
							if (total_dims < dims.size())
							{
								this->log << "error array axes: <" << name << ">, your"
									<< dims.size() << "are more than aim'" << total_dims
									<< "\n";
								throw SemanticError(ParserErrorCode::Stop);
							};

							{
								for (size_t i = 1; i < dims.size(); ++i)
								{
									if (arr.dims[i] <= dims[i])
									{
										this->log << "error array dimension-" << i << ": <" << name << ">, yours has "
											<< dims[i] << "however aim has" << arr.dims[i]
											<< "\n";
										throw SemanticError(ParserErrorCode::Stop);
									}
								}
							}

							uint32_t stride = arr.stride;
							auto current_stride = stride;
							uint32_t local_offset = 0;
							for (size_t i = 0; i < dims.size(); ++i)
							{
								current_stride = stride;
								for (size_t j = i + 1; j < total_dims; ++j)
									current_stride *= arr.dims[j];
								local_offset += dims[i] * current_stride;
							}
							offset += local_offset;
							size = current_stride;
							if (dims.size() < total_dims) inArray = true;
						}

					}
					break;
					default:break;
					}
					return PassOn(0);
				});
		}

		{
			auto& p = CreateParseRule();
			p.expression = "Member -> Member . Array";
			p.SetAction(PassOn(2));
		}

		{
			auto& p = CreateParseRule();
			p.expression = "Member -> Member . Mem";
			p.SetAction(PassOn(2));
		}

		{
			auto& p = CreateParseRule();
			p.expression = "Member -> Array";
			p.SetAction(PassOn(0));
		}
	}

	void test()
	{
		CommandBuffer cmd;
		PipelineLayout layout;
		std::vector<char> values;
		cmd.pushConstants(layout, vk::ShaderStageFlagBits::eVertex, 0, 10, values.data());
		spv_reflect::ShaderModule m;
		SpvReflectDescriptorSet s;
		SpvReflectDescriptorBinding b;
		b.name;
		vk::DescriptorSet set;
		vk::WriteDescriptorSet w(set, 0, 0, vk::DescriptorType::eStorageBuffer, {}, {}, {});
	}

	EntryGen entryGen;
}

