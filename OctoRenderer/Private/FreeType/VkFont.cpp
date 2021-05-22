/*
*	Copyright 2015-2016 Tomas Mikalauskas. All rights reserved.
*	GitHub repository - https://github.com/TywyllSoftware/TywRenderer
*	This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/
#include <RendererPch\stdafx.h>


#include "Geometry\VertData.h"
#include "Geometry\JointTransform.h"

//Renderer Includes
#include "Vulkan\VulkanTools.h"
#include "Vulkan\VkBufferObject.h"
#include "Vulkan\VulkanTextureLoader.h"
#include "Vulkan\VkTexture2D.h"

//Main Renderer
#include "VKRenderer.h"

#include "ThirdParty\FreeType\FreetypeLoad.h"
#include "VkFont.h"


//math
#include <External\glm\glm\gtc\matrix_inverse.hpp>


/*
=======================================
VkFont::VkFont
=======================================
*/

VkFont::VkFont(VkPhysicalDevice physicalDevice,
	VkDevice device,
	VkQueue queue,
	std::vector<VkFramebuffer> &framebuffers,
	VkFormat colorformat,
	VkFormat depthformat,
	uint32_t *framebufferwidth,
	uint32_t *framebufferheight)
	:data(TYW_NEW GlyphData())
{
	this->physicalDevice = physicalDevice;
	this->device = device;
	this->queue = queue;
	this->colorFormat = colorformat;
	this->depthFormat = depthformat;

	this->frameBuffers.resize(framebuffers.size());
	for (uint32_t i = 0; i < framebuffers.size(); i++)
	{
		this->frameBuffers[i] = &framebuffers[i];
	}
	this->frameBufferWidth = framebufferwidth;
	this->frameBufferHeight = framebufferheight;

	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);
}

/*
=======================================
VkFont::CreateVkFont
=======================================
*/
void VkFont::CreateFontVk(const std::string& strTypeface, int point_size, int dpi) {
	if (!data->LoadGlyph(strTypeface.c_str(), point_size, dpi)) 
	{
		printf("%s \n", data->getLog().c_str());
		return;
	}
}

/*
=======================================
VkFont::InitializeChars
=======================================
*/
void VkFont::InitializeChars(char* source, VkTools::VulkanTextureLoader& pTextureLoader)
{
	if (!data->InitiliazeChars(source)) 
	{
		printf("%s \n", data->getLog().c_str());
		printf("ERROR: VkFont::InitializeChars: returned false \n");
	}
	
	//SetupDescriptorPool();
	//SetupDescriptorSetLayout();
	VkDescriptorSetAllocateInfo allocInfo = VkTools::Initializer::DescriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);

	size_t size = strlen(source);
	for (int i = 0; i < size; i++) 
	{
		Data temp = data->getChar(source[i]);
		if (temp.bitmap_buffer != nullptr && temp.c == source[i]) 
		{
			VkTools::VulkanTexture* texture = TYW_NEW VkTools::VulkanTexture;
			
			pTextureLoader.GenerateTexture(temp.bitmap_buffer, texture, VkFormat::VK_FORMAT_R8_UNORM, sizeof(temp.bitmap_buffer), temp.bitmap_width, temp.bitmap_rows, 1, false,  VK_IMAGE_USAGE_SAMPLED_BIT);
			glyphs.insert(std::unordered_map<char, VkTools::VulkanTexture*>::value_type(source[i], texture));



			VkDescriptorSet set;
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &set));
			VkDescriptorImageInfo texDescriptorDiffuse = VkTools::Initializer::DescriptorImageInfo(texture->sampler, texture->view, VK_IMAGE_LAYOUT_GENERAL);
			std::vector<VkWriteDescriptorSet> writeDescriptorSets =
			{
				// Binding 0 : Vertex shader uniform buffer
				VkTools::Initializer::WriteDescriptorSet(set,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,	0,&uniformDataVS.descriptor),

				//// Binding 1 : Fragment shader texture sampler
				VkTools::Initializer::WriteDescriptorSet(set,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,&texDescriptorDiffuse),
			};

			vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
			descriptors.insert(std::unordered_map<char, VkDescriptorSet>::value_type(source[i], set));
		}
		else 
		{
			printf("ERROR: Could not find char %c\n", source[i]);
		}
	}
	//I do not need anymore glyph buffer data, so I delete it
	data->ReleaseBuffer();
}


/*
=======================================
VkFont::DisplayText()
=======================================
*/
void VkFont::AddText(float x, float y, float ws, float hs, const std::string& text)
{
	this->text += text;
	for (int i = 0; i < this->text.size(); i++)
	{

		if (this->text[i] == ' ')
		{
			x += 32*ws;
			continue;
		}

		Data currentGlyph = data->getChar(text[i]);
		if (currentGlyph.c != text[i]) 
		{
			printf("Could not find char %c \n", text[i]);
			return;
		}

		float vx = x + currentGlyph.bitmap_left * ws;
		float vy = y + currentGlyph.bitmap_top * hs;
		float w = currentGlyph.bitmap_width * ws;
		float h = currentGlyph.bitmap_rows * hs;
		
		drawFont data[6] =
		{
			{  glm::vec3(vx,vy,    1),	 glm::vec2(0,0) },
			{  glm::vec3(vx,vy-h,  1),   glm::vec2(0,1) },
			{  glm::vec3(vx+w,vy,  1),   glm::vec2(1,0) },

			{  glm::vec3(vx+w,vy,  1),   glm::vec2(1,0) },
			{  glm::vec3(vx,vy-h,  1),   glm::vec2(0,1) },
			{  glm::vec3(vx+w,vy-h,1),   glm::vec2(1,1) }
		};


		{
			pDataLocal->tex = data[0].tex;
			pDataLocal->vertex = data[0].vertex;
			pDataLocal++;

			pDataLocal->tex = data[1].tex;
			pDataLocal->vertex = data[1].vertex;
			pDataLocal++;

			pDataLocal->tex = data[2].tex;
			pDataLocal->vertex = data[2].vertex;
			pDataLocal++;

			pDataLocal->tex = data[3].tex;
			pDataLocal->vertex = data[3].vertex;
			pDataLocal++;

			pDataLocal->tex = data[4].tex;
			pDataLocal->vertex = data[4].vertex;
			pDataLocal++;

			pDataLocal->tex = data[5].tex;
			pDataLocal->vertex = data[5].vertex;
			pDataLocal++;


			numLetters++;
		}



		// increment position /
		x += (currentGlyph.advance.x >> 6) * ws;
		y += (currentGlyph.advance.y >> 6) * hs;
	}
}


/*
=============================
~VkFont
=============================
*/
VkFont::~VkFont() 
{
	//Delete textures
	for (auto& texture : glyphs)
	{
		VkTools::VulkanTexture* pTexture = texture.second;

		vkDestroyImageView(device, pTexture->view, nullptr);
		vkDestroyImage(device, pTexture->image, nullptr);
		vkFreeMemory(device, pTexture->deviceMemory, nullptr);
		vkDestroySampler(device, pTexture->sampler, nullptr);
	}

	//Delete data
	VkBufferObject::DeleteBufferMemory(device, m_BufferData, nullptr);
	vkDestroyBuffer(device, uniformDataVS.buffer, nullptr);
	vkFreeMemory(device, uniformDataVS.memory, nullptr);

	//check if something went wrong
	assert(data->Release() && "Could not delete GlyphData");

	//delete glyph data
	SAFE_DELETE(data);

	//Destroy Shader Modules
	for (int i = 0; i < m_ShaderModules.size(); i++)
	{
		vkDestroyShaderModule(device, m_ShaderModules[i], nullptr);
	}

	if (commandPool != VK_NULL_HANDLE)
	{
		//Delete Commands
		vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(cmdBuffers.size()), cmdBuffers.data());
	}


	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyRenderPass(device, renderPass, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout , nullptr);

	//Desotry descriptor layout
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

	//Destroy cache and pool
	vkDestroyPipelineCache(device, pipelineCache, nullptr);
	vkDestroyCommandPool(device, commandPool, nullptr);
	vkDestroyDescriptorPool(device, descriptorPool, nullptr);
}


void VkFont::SetupDescriptorSetLayout()
{
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
	{
		// Binding 0 : Vertex shader uniform buffer
		VkTools::Initializer::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,VK_SHADER_STAGE_VERTEX_BIT,0),

		//// Binding 1 : Fragment shader image sampler
		VkTools::Initializer::DescriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,VK_SHADER_STAGE_FRAGMENT_BIT,1),
	};

	VkDescriptorSetLayoutCreateInfo descriptorLayout = VkTools::Initializer::DescriptorSetLayoutCreateInfo(setLayoutBindings.data(), setLayoutBindings.size());
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

	VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = VkTools::Initializer::PipelineLayoutCreateInfo(&descriptorSetLayout, 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
}


void VkFont::SetupDescriptorPool()
{
	// Example uses one ubo and one image sampler
	std::vector<VkDescriptorPoolSize> poolSizes =
	{
		VkTools::Initializer::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256),
		VkTools::Initializer::DescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 256)
	};

	VkDescriptorPoolCreateInfo descriptorPoolInfo = VkTools::Initializer::DescriptorPoolCreateInfo(poolSizes.size(), poolSizes.data(), 66);
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
}


void VkFont::PrepareRenderPass()
{
	VkAttachmentDescription attachments[2] = {};

	// Color attachment
	attachments[0].format = colorFormat;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	// Don't clear the framebuffer (like the renderpass from the example does)
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Depth attachment
	attachments[1].format = depthFormat;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorReference = {};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 1;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.flags = 0;
	subpass.inputAttachmentCount = 0;
	subpass.pInputAttachments = NULL;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorReference;
	subpass.pResolveAttachments = NULL;
	subpass.pDepthStencilAttachment = &depthReference;
	subpass.preserveAttachmentCount = 0;
	subpass.pPreserveAttachments = NULL;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.pNext = NULL;
	renderPassInfo.attachmentCount = 2;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 0;
	renderPassInfo.pDependencies = NULL;

	VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
}


void VkFont::PreparePipeline()
{
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.layout = pipelineLayout;
	pipelineCreateInfo.renderPass = renderPass;

	// Vertex input state
	// Describes the topoloy used with this pipeline
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
	inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyState.flags = 0;
	inputAssemblyState.primitiveRestartEnable = VK_FALSE;

	// Rasterization state
	VkPipelineRasterizationStateCreateInfo rasterizationState = {};
	rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizationState.flags = 0;
	rasterizationState.depthClampEnable = VK_TRUE;
	rasterizationState.lineWidth = 1.0f;

	// Enable blending
	VkPipelineColorBlendAttachmentState blendAttachmentState = {};
	blendAttachmentState.colorWriteMask = 0xf, VK_TRUE;
	blendAttachmentState.blendEnable = VK_TRUE;

	blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	blendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;


	// Color blend state
	VkPipelineColorBlendStateCreateInfo colorBlendState = {};
	colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendState.pNext = NULL;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = &blendAttachmentState;


	// Viewport state
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	// One viewport
	viewportState.viewportCount = 1;
	// One scissor rectangle
	viewportState.scissorCount = 1;

	// Enable dynamic states
	// Describes the dynamic states to be used with this pipeline
	// Dynamic states can be set even after the pipeline has been created
	// So there is no need to create new pipelines just for changing
	// a viewport's dimensions or a scissor box
	VkPipelineDynamicStateCreateInfo dynamicState = {};
	// The dynamic state properties themselves are stored in the command buffer
	std::vector<VkDynamicState> dynamicStateEnables;
	dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
	dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.pDynamicStates = dynamicStateEnables.data();
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());

	// Depth and stencil state
	// Describes depth and stenctil test and compare ops
	VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
	// Basic depth compare setup with depth writes and depth test enabled
	// No stencil used 
	depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilState.depthTestEnable = VK_TRUE;
	depthStencilState.depthWriteEnable = VK_TRUE;
	depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencilState.depthBoundsTestEnable = VK_FALSE;
	depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
	depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
	depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
	depthStencilState.stencilTestEnable = VK_FALSE;
	depthStencilState.front = depthStencilState.back;

	// Multi sampling state
	VkPipelineMultisampleStateCreateInfo multisampleState = {};
	multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleState.pSampleMask = NULL;
	// No multi sampling used in this example
	multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// Load shaders
	//Shaders are loaded from the SPIR-V format, which can be generated from glsl
	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;



	shaderStages[0] = LoadShader(VKRenderer::GetAssetPath() + "Shaders/FontRendering/FontRendering.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = LoadShader(VKRenderer::GetAssetPath() + "Shaders/FontRendering/FontRendering.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);


	// Create Pipeline state VI-IA-VS-VP-RS-FS-CB
	pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineCreateInfo.pStages = shaderStages.data();
	pipelineCreateInfo.pVertexInputState = &m_BufferData.inputState;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.renderPass = renderPass;
	pipelineCreateInfo.pDynamicState = &dynamicState;

	// Create rendering pipeline
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline));
}


VkPipelineShaderStageCreateInfo VkFont::LoadShader(const std::string& fileName, VkShaderStageFlagBits stage)
{
	VkPipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = stage;
#if defined(__ANDROID__)
	shaderStage.module = vkTools::loadShader(androidApp->activity->assetManager, fileName.c_str(), device, stage);
#else
	shaderStage.module = VkTools::LoadShader(fileName.c_str(), device, stage);
#endif
	shaderStage.pName = "main"; // todo : make param
	assert(shaderStage.module != NULL);
	m_ShaderModules.push_back(shaderStage.module);
	return shaderStage;
}


void VkFont::PrepareResources(uint32_t width, uint32_t height)
{
	// Pool
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = 0; // todo : pass from example base / swap chain
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &commandPool));

	//Allocate cmd buffer size
	cmdBuffers.resize(frameBuffers.size());

	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		VkTools::Initializer::CommandBufferAllocateInfo(
			commandPool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			(uint32_t)cmdBuffers.size());

	
	VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, cmdBuffers.data()));


	// Vertex buffer
	VkDeviceSize bufferSize = 256 * sizeof(drawFont);

	VkBufferCreateInfo bufferInfo = VkTools::Initializer::BufferCreateInfo(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, bufferSize);
	VK_CHECK_RESULT(vkCreateBuffer(device, &bufferInfo, nullptr, &m_BufferData.buffer));

	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo allocInfo = VkTools::Initializer::MemoryAllocateInfo();

	vkGetBufferMemoryRequirements(device, m_BufferData.buffer, &memReqs);
	allocInfo.allocationSize = memReqs.size;
	allocInfo.memoryTypeIndex = VkTools::GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, deviceMemoryProperties);
	VK_CHECK_RESULT(vkAllocateMemory(device, &allocInfo, nullptr, &m_BufferData.memory));
	VK_CHECK_RESULT(vkBindBufferMemory(device, m_BufferData.buffer, m_BufferData.memory, 0));

	

	//Allocate descriptors
	VkBufferObject::BindVertexUvDescriptor(m_BufferData);


	//// Pipeline cache
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));


	PrepareRenderPass();
	PreparePipeline();
	UpdateUniformBuffers(width, height, 0);
}



void VkFont::BeginTextUpdate()
{
	// Map persistent
	VK_CHECK_RESULT(vkMapMemory(device, m_BufferData.memory, 0, VK_WHOLE_SIZE, 0, (void **)&pData));

	pDataLocal = pData;
	numLetters = 0;
	text.clear();
}

void VkFont::EndTextUpdate()
{
	//Unmap memory
	vkUnmapMemory(device, m_BufferData.memory);
	pDataLocal = nullptr;
	pData = nullptr;

	static VkClearValue clearValue{ 0.0f, 0.0f, 0.0f, 0.0f };

	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = NULL;
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = *frameBufferWidth;
	renderPassBeginInfo.renderArea.extent.height = *frameBufferHeight;
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = &clearValue;

	VkCommandBufferBeginInfo cmdBufInfo = VkTools::Initializer::CommandBufferBeginInfo();
	for (int32_t i = 0; i < cmdBuffers.size(); ++i)
	{
		renderPassBeginInfo.framebuffer = *frameBuffers[i];

		VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffers[i], &cmdBufInfo));
		vkCmdBeginRenderPass(cmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);


		// Update dynamic viewport state
		VkViewport viewport = {};
		viewport.height = static_cast<float>(*frameBufferHeight);
		viewport.width = static_cast<float>(*frameBufferWidth);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(cmdBuffers[i], 0, 1, &viewport);

		// Update dynamic scissor state
		VkRect2D scissor = {};
		scissor.extent.width = *frameBufferWidth;
		scissor.extent.height = *frameBufferHeight;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vkCmdSetScissor(cmdBuffers[i], 0, 1, &scissor);

		VkDeviceSize vertex_offset = 0;
		vkCmdBindPipeline(cmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vkCmdBindVertexBuffers(cmdBuffers[i], 0, 1, &m_BufferData.buffer, &vertex_offset);

		uint32_t char_offset = 0;
		for (uint32_t j = 0; j < text.size(); j++)
		{
			if (text[j] == ' ')continue; //skip space

			char_offset = j * 6;
			vkCmdBindDescriptorSets(cmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptors[text[j]], 0, NULL);
			vkCmdDraw(cmdBuffers[i], 6, 1, char_offset,0);
		}
		vkCmdEndRenderPass(cmdBuffers[i]);
		VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuffers[i]));
	}
}



// Submit the text command buffers to a queue
void VkFont::SubmitToQueue(VkQueue queue, uint32_t bufferindex)
{
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; submitInfo.commandBufferCount = 1;;
	submitInfo.pCommandBuffers = &cmdBuffers[bufferindex];
	submitInfo.commandBufferCount = 1;

	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK_RESULT(vkQueueWaitIdle(queue));
}


void VkFont::UpdateUniformBuffers(uint32_t windowWidth, uint32_t windowHeight, float zoom)
{
	// Update matrices
	m_uboVS.projectionMatrix = glm::perspective(glm::radians(60.0f), (float)windowWidth / (float)windowHeight, 0.1f, 256.0f);

	m_uboVS.viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, 0.0f));
	m_uboVS.modelMatrix = m_uboVS.viewMatrix * glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, -1.5));

	// Map uniform buffer and update it
	uint8_t *pData;
	VK_CHECK_RESULT(vkMapMemory(device, uniformDataVS.memory, 0, sizeof(m_uboVS), 0, (void **)&pData));
	memcpy(pData, &m_uboVS, sizeof(m_uboVS));
	vkUnmapMemory(device, uniformDataVS.memory);
}


void VkFont::PrepareUniformBuffers()
{
	// Prepare and initialize a uniform buffer block containing shader uniforms
	// In Vulkan there are no more single uniforms like in GL
	// All shader uniforms are passed as uniform buffer blocks 
	VkMemoryRequirements memReqs;

	// Vertex shader uniform buffer block
	VkBufferCreateInfo bufferInfo = {};
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = NULL;
	allocInfo.allocationSize = 0;
	allocInfo.memoryTypeIndex = 0;

	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = sizeof(m_uboVS);
	bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	// Create a new buffer
	VK_CHECK_RESULT(vkCreateBuffer(device, &bufferInfo, nullptr, &uniformDataVS.buffer));
	// Get memory requirements including size, alignment and memory type 
	vkGetBufferMemoryRequirements(device, uniformDataVS.buffer, &memReqs);
	allocInfo.allocationSize = memReqs.size;
	// Get the memory type index that supports host visibile memory access
	// Most implementations offer multiple memory tpyes and selecting the 
	// correct one to allocate memory from is important
	// We also want the buffer to be host coherent so we don't have to flush 
	// after every update. 
	// Note that this may affect performance so you might not want to do this 
	// in a real world application that updates buffers on a regular base
	allocInfo.memoryTypeIndex = VkTools::GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, deviceMemoryProperties);
	// Allocate memory for the uniform buffer
	VK_CHECK_RESULT(vkAllocateMemory(device, &allocInfo, nullptr, &(uniformDataVS.memory)));
	// Bind memory to buffer
	VK_CHECK_RESULT(vkBindBufferMemory(device, uniformDataVS.buffer, uniformDataVS.memory, 0));

	// Store information in the uniform's descriptor
	uniformDataVS.descriptor.buffer = uniformDataVS.buffer;
	uniformDataVS.descriptor.offset = 0;
	uniformDataVS.descriptor.range = sizeof(m_uboVS);
}


void VkFont::Resize(uint32_t windowWidth, uint32_t windowHeight)
{
}