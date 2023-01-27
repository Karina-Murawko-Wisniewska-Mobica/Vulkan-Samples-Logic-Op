/* Copyright (c) 2023, Mobica Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "logic_op_dynamic_state.h"

//#include "common/vk_common.h"
//#include "gltf_loader.h"
//#include "gui.h"
//#include "platform/filesystem.h"
//#include "platform/platform.h"
//#include "rendering/subpasses/forward_subpass.h"
//#include "stats/stats.h"

#include "gltf_loader.h"
#include "scene_graph/components/mesh.h"
#include "scene_graph/components/sub_mesh.h"
#include "scene_graph/components/pbr_material.h"

LogicOpDynamicState::LogicOpDynamicState()
{
	title = "Logic Operations Dynamic State";

	add_instance_extension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
	add_device_extension(VK_EXT_EXTENDED_DYNAMIC_STATE_2_EXTENSION_NAME);
	add_device_extension(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
}

LogicOpDynamicState::~LogicOpDynamicState()
{
	if (device)
	{
		//vkDestroySampler(get_device().get_handle(), textures.envmap.sampler, VK_NULL_HANDLE);
		textures = {};
		background_model.reset();
		model.reset();
		ubo.reset();
				
		vkDestroyPipeline(get_device().get_handle(), pipeline.background, VK_NULL_HANDLE);
		vkDestroyPipeline(get_device().get_handle(), pipeline.model, VK_NULL_HANDLE);
		
		vkDestroyPipelineLayout(get_device().get_handle(), pipeline_layouts.background, VK_NULL_HANDLE);
		vkDestroyPipelineLayout(get_device().get_handle(), pipeline_layouts.model, VK_NULL_HANDLE);

		vkDestroyDescriptorSetLayout(get_device().get_handle(), descriptor_set_layouts.model, VK_NULL_HANDLE);
		vkDestroyDescriptorSetLayout(get_device().get_handle(), descriptor_set_layouts.background, VK_NULL_HANDLE);
	}
}

/**
 * 	@fn bool LogicOpDynamicState::prepare(vkb::Platform &platform)
 * 	@brief Configuring all sample specific settings, creating descriptor sets/pool, pipelines, generating or loading models etc.
 */
bool LogicOpDynamicState::prepare(vkb::Platform &platform)
{
	if (!ApiVulkanSample::prepare(platform))
	{
		return false;
	}

	// Attach a move script to the camera component in the scene
	//auto &camera_node = vkb::add_free_camera(*scene, "main_camera", get_render_context().get_surface_extent());
	//auto  camera      = &camera_node.get_component<vkb::sg::Camera>();
	camera.type = vkb::CameraType::LookAt;
	camera.set_position({2.0f, -4.0f, -10.0f});
	camera.set_rotation({-15.0f, 190.0f, 0.0f});
	camera.set_perspective(60.0f, static_cast<float>(width) / static_cast<float>(height), 256.0f, 0.1f);

	load_assets();
	prepare_uniform_buffers();
	create_descriptor_pool();

	setup_descriptor_set_layout();
	create_descriptor_sets();

	create_pipeline();
	build_command_buffers();

	// Add a GUI with the stats you want to monitor
	// TODO uncomment: currently it crashes
	// stats->request_stats({/*stats you require*/});
	// gui = std::make_unique<vkb::Gui>(*this, platform.get_window(), stats.get());

	return true;
}

void LogicOpDynamicState::create_render_context(vkb::Platform &platform)
{
	// We always want an sRGB surface to match the display.
	// If we used a UNORM surface, we'd have to do the conversion to sRGB ourselves at the end of our fragment shaders.
	auto surface_priority_list = std::vector<VkSurfaceFormatKHR>{{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
	                                                             {VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};

	render_context = platform.create_render_context(*device.get(), surface, surface_priority_list);
}

/**
 * 	@fn void LogicOpDynamicState::render(float delta_time)
 * 	@brief Drawing frames and/or updating uniform buffers when camera position/rotation was changed
 */
void LogicOpDynamicState::render(float delta_time)
{
	if (!prepared)
	{
		return;
	}
	draw();
	if (camera.updated)
	{
		update_uniform_buffers();
	}
}

/**
 * 	@fn void LogicOpDynamicState::build_command_buffers()
 * 	@brief Creating command buffers and drawing particular elements on window.
 */
void LogicOpDynamicState::build_command_buffers()
{
	std::array<VkClearValue, 2> clear_values{};
	clear_values[0].color        = {{0.0f, 0.0f, 0.0f, 0.0f}};
	clear_values[1].depthStencil = {0.0f, 0};

	int i = -1;
	for (auto &draw_cmd_buffer : draw_cmd_buffers)
	{
		++i;
		auto command_begin = vkb::initializers::command_buffer_begin_info();
		VK_CHECK(vkBeginCommandBuffer(draw_cmd_buffer, &command_begin));

		VkRenderPassBeginInfo render_pass_begin_info    = vkb::initializers::render_pass_begin_info();
		render_pass_begin_info.renderPass               = render_pass;
		render_pass_begin_info.framebuffer              = framebuffers[i];
		render_pass_begin_info.renderArea.extent.width  = width;
		render_pass_begin_info.renderArea.extent.height = height;
		render_pass_begin_info.clearValueCount          = static_cast<uint32_t>(clear_values.size());
		render_pass_begin_info.pClearValues             = clear_values.data();

		vkCmdBeginRenderPass(draw_cmd_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
		VkViewport viewport = vkb::initializers::viewport(static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f);
		vkCmdSetViewport(draw_cmd_buffer, 0, 1, &viewport);

		VkRect2D scissor = vkb::initializers::rect2D(static_cast<int>(width), static_cast<int>(height), 0, 0);
		vkCmdSetScissor(draw_cmd_buffer, 0, 1, &scissor);

		/* Binding baseline pipeline and descriptor sets */
		vkCmdBindDescriptorSets(draw_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layouts.model, 0, 1, &descriptor_sets.model, 0, VK_NULL_HANDLE);
		vkCmdBindPipeline(draw_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.model);

		/* Setting topology to triangle list and disabling primitive restart functionality */
		vkCmdSetPrimitiveTopologyEXT(draw_cmd_buffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
		vkCmdSetPrimitiveRestartEnableEXT(draw_cmd_buffer, VK_FALSE);

		/* Drawing objects from baseline scene (with rasterizer discard and depth bias functionality) */
		draw_from_scene(draw_cmd_buffer, scene_elements_model);

		vkCmdBindDescriptorSets(draw_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layouts.background, 0, 1, &descriptor_sets.background, 0, VK_NULL_HANDLE);
		vkCmdBindPipeline(draw_cmd_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.background);

		/* Drawing background */
		draw_model(background_model, draw_cmd_buffer);

		/* UI */
		draw_ui(draw_cmd_buffer);

		vkCmdEndRenderPass(draw_cmd_buffer);

		VK_CHECK(vkEndCommandBuffer(draw_cmd_buffer));
	}
}

/**
 * @fn void LogicOpDynamicState::request_gpu_features(vkb::PhysicalDevice &gpu)
 * @brief Enabling features related to Vulkan extensions
 */
void LogicOpDynamicState::request_gpu_features(vkb::PhysicalDevice &gpu)
{
	/* Enable extension features required by this sample
	   These are passed to device creation via a pNext structure chain */
	auto &requested_extended_dynamic_state2_features                        = gpu.request_extension_features<VkPhysicalDeviceExtendedDynamicState2FeaturesEXT>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT);
	requested_extended_dynamic_state2_features.extendedDynamicState2        = VK_TRUE;
	requested_extended_dynamic_state2_features.extendedDynamicState2LogicOp = VK_TRUE;

	auto &requested_extended_dynamic_state_feature                = gpu.request_extension_features<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT);
	requested_extended_dynamic_state_feature.extendedDynamicState = VK_TRUE;

	if (gpu.get_features().samplerAnisotropy)
	{
		gpu.get_mutable_requested_features().samplerAnisotropy = VK_TRUE;
	}
}

void LogicOpDynamicState::prepare_uniform_buffers()
{
	uniform_buffers.model = std::make_unique<vkb::core::Buffer>(get_device(), sizeof(ubo_model), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	uniform_buffers.background  = std::make_unique<vkb::core::Buffer>(get_device(), sizeof(ubo_background), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	
	update_uniform_buffers();
}

void LogicOpDynamicState::update_uniform_buffers()
{
	/* Baseline uniform buffer */
	ubo_model.projection = camera.matrices.perspective;
	ubo_model.view       = camera.matrices.view;
	uniform_buffers.model->convert_and_update(ubo_model);

	/* Background uniform buffer */
	ubo_background.projection           = camera.matrices.perspective;
	ubo_background.background_modelview = camera.matrices.view;

	uniform_buffers.background->convert_and_update(ubo_background);
}

void LogicOpDynamicState::create_pipeline()
{
	/* Setup for first pipeline */
	VkPipelineInputAssemblyStateCreateInfo input_assembly_state =
	    vkb::initializers::pipeline_input_assembly_state_create_info(
	        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	        0,
	        VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterization_state =
	    vkb::initializers::pipeline_rasterization_state_create_info(
	        VK_POLYGON_MODE_FILL,
	        VK_CULL_MODE_BACK_BIT,
	        VK_FRONT_FACE_CLOCKWISE,
	        0);

	rasterization_state.depthBiasConstantFactor = 1.0f;
	rasterization_state.depthBiasSlopeFactor    = 1.0f;

	VkPipelineColorBlendAttachmentState blend_attachment_state =
	    vkb::initializers::pipeline_color_blend_attachment_state(
	        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
	        VK_TRUE);

	blend_attachment_state.colorBlendOp        = VK_BLEND_OP_ADD;
	blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blend_attachment_state.alphaBlendOp        = VK_BLEND_OP_ADD;
	blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

	VkPipelineColorBlendStateCreateInfo color_blend_state =
	    vkb::initializers::pipeline_color_blend_state_create_info(
	        1,
	        &blend_attachment_state);

	/* Note: Using Reversed depth-buffer for increased precision, so Greater depth values are kept */
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state =
	    vkb::initializers::pipeline_depth_stencil_state_create_info(
	        VK_TRUE,
	        VK_TRUE,
	        VK_COMPARE_OP_GREATER);

	VkPipelineViewportStateCreateInfo viewport_state =
	    vkb::initializers::pipeline_viewport_state_create_info(1, 1, 0);

	VkPipelineMultisampleStateCreateInfo multisample_state =
	    vkb::initializers::pipeline_multisample_state_create_info(
	        VK_SAMPLE_COUNT_1_BIT,
	        0);

	std::vector<VkDynamicState> dynamic_state_enables = {
	    VK_DYNAMIC_STATE_VIEWPORT,
	    VK_DYNAMIC_STATE_SCISSOR,
	    VK_DYNAMIC_STATE_LOGIC_OP_EXT
	};

	VkPipelineDynamicStateCreateInfo dynamic_state =
	    vkb::initializers::pipeline_dynamic_state_create_info(
	        dynamic_state_enables.data(),
	        static_cast<uint32_t>(dynamic_state_enables.size()),
	        0);

	/* Binding description */
	std::vector<VkVertexInputBindingDescription> vertex_input_bindings = {
	    vkb::initializers::vertex_input_binding_description(0, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX),
	    vkb::initializers::vertex_input_binding_description(1, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX)
	};

	/* Attribute descriptions */
	std::vector<VkVertexInputAttributeDescription> vertex_input_attributes = {
	    vkb::initializers::vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),        // Position
	    vkb::initializers::vertex_input_attribute_description(1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0),        // Normal
	};

	VkPipelineVertexInputStateCreateInfo vertex_input_state = vkb::initializers::pipeline_vertex_input_state_create_info();
	vertex_input_state.vertexBindingDescriptionCount        = static_cast<uint32_t>(vertex_input_bindings.size());
	vertex_input_state.pVertexBindingDescriptions           = vertex_input_bindings.data();
	vertex_input_state.vertexAttributeDescriptionCount      = static_cast<uint32_t>(vertex_input_attributes.size());
	vertex_input_state.pVertexAttributeDescriptions         = vertex_input_attributes.data();

	//rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{};
	shader_stages[0] = load_shader("extended_dynamic_state2/model.vert", VK_SHADER_STAGE_VERTEX_BIT);
	shader_stages[1] = load_shader("extended_dynamic_state2/model.frag", VK_SHADER_STAGE_FRAGMENT_BIT);

	shader_stages[0] = load_shader("extended_dynamic_state2/background.vert", VK_SHADER_STAGE_VERTEX_BIT);
	shader_stages[1] = load_shader("extended_dynamic_state2/background.frag", VK_SHADER_STAGE_FRAGMENT_BIT);

	/* Use the pNext to point to the rendering create struct */
	VkGraphicsPipelineCreateInfo graphics_create{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
	graphics_create.pNext               = VK_NULL_HANDLE;
	graphics_create.renderPass          = VK_NULL_HANDLE;
	graphics_create.pInputAssemblyState = &input_assembly_state;
	graphics_create.pRasterizationState = &rasterization_state;
	graphics_create.pColorBlendState    = &color_blend_state;
	graphics_create.pMultisampleState   = &multisample_state;
	graphics_create.pViewportState      = &viewport_state;
	graphics_create.pDepthStencilState  = &depth_stencil_state;
	graphics_create.pDynamicState       = &dynamic_state;
	graphics_create.pVertexInputState   = &vertex_input_state;
	graphics_create.pTessellationState  = VK_NULL_HANDLE;
	graphics_create.stageCount          = 2;
	graphics_create.pStages             = shader_stages.data();
	graphics_create.layout              = pipeline_layouts.model;

	graphics_create.pNext      = VK_NULL_HANDLE;
	graphics_create.renderPass = render_pass;

	//pipeline_cache

	VK_CHECK(vkCreateGraphicsPipelines(get_device().get_handle(), VK_NULL_HANDLE, 1, &graphics_create, VK_NULL_HANDLE, &pipeline.model));

	/* Setup for second pipeline */
	graphics_create.layout = pipeline_layouts.background;

	std::vector<VkDynamicState> dynamic_state_enables_background = {
	    VK_DYNAMIC_STATE_VIEWPORT,
	    VK_DYNAMIC_STATE_SCISSOR,
	};
	dynamic_state.pDynamicStates    = dynamic_state_enables_background.data();
	dynamic_state.dynamicStateCount = static_cast<uint32_t>(dynamic_state_enables_background.size());

	/* Binding description */
	std::vector<VkVertexInputBindingDescription> vertex_input_bindings_background = {
	    vkb::initializers::vertex_input_binding_description(0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX),
	};

	/* Attribute descriptions */
	std::vector<VkVertexInputAttributeDescription> vertex_input_attributes_background = {
	    vkb::initializers::vertex_input_attribute_description(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),                               // Position
	    vkb::initializers::vertex_input_attribute_description(0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)),        // Normal
	};

	vertex_input_state.vertexBindingDescriptionCount   = static_cast<uint32_t>(vertex_input_bindings_background.size());
	vertex_input_state.pVertexBindingDescriptions      = vertex_input_bindings_background.data();
	vertex_input_state.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_input_attributes_background.size());
	vertex_input_state.pVertexAttributeDescriptions    = vertex_input_attributes_background.data();

	rasterization_state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	shader_stages[0] = load_shader("extended_dynamic_state2/background.vert", VK_SHADER_STAGE_VERTEX_BIT);
	shader_stages[1] = load_shader("extended_dynamic_state2/background.frag", VK_SHADER_STAGE_FRAGMENT_BIT);

	VK_CHECK(vkCreateGraphicsPipelines(get_device().get_handle(), pipeline_cache, 1, &graphics_create, VK_NULL_HANDLE, &pipeline.background));
}

void LogicOpDynamicState::draw()
{
	ApiVulkanSample::prepare_frame();
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers    = &draw_cmd_buffers[current_buffer];
	VK_CHECK(vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE));
	ApiVulkanSample::submit_frame();
}

/**
 * 	@fn void LogicOpDynamicState::load_assets()
 *	@brief Loading extra models, textures from assets
 */
void LogicOpDynamicState::load_assets()
{
	/* Models */
	//model = load_model("scenes/cube.gltf");

	load_scene("scenes/primitives/primitives.gltf");

	//scene_elements_model
	// 
	//std::vector<SceneNode> scene_elements;
	// Store all scene nodes in a linear vector for easier access
	for (auto &mesh : scene->get_components<vkb::sg::Mesh>())
	{
		for (auto &node : mesh->get_nodes())
		{
			for (auto &sub_mesh : mesh->get_submeshes())
			{
				scene_elements_model.push_back({mesh->get_name(), node, sub_mesh});
			}
		}
	}
	///* Split scene */
	//scene_pipeline_divide(scene_elements);

	background_model = load_model("scenes/cube.gltf");

	/* Load HDR cube map */
	textures.envmap = load_texture_cubemap("textures/uffizi_rgba16f_cube.ktx", vkb::sg::Image::Color);
}

//TODO
void LogicOpDynamicState::create_descriptor_pool()
{
	constexpr uint32_t num_descriptor_sets = 2;

	std::vector<VkDescriptorPoolSize> pool_sizes = {
        vkb::initializers::descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, num_descriptor_sets),
	    vkb::initializers::descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1),
	};
	
	VkDescriptorPoolCreateInfo descriptor_pool_create_info =
	    vkb::initializers::descriptor_pool_create_info(
	        static_cast<uint32_t>(pool_sizes.size()),
	        pool_sizes.data(),
	        num_descriptor_sets);

	VK_CHECK(vkCreateDescriptorPool(get_device().get_handle(), &descriptor_pool_create_info, VK_NULL_HANDLE, &descriptor_pool));
}

void LogicOpDynamicState::setup_descriptor_set_layout()
{
	/* Model */
	std::vector<VkDescriptorSetLayoutBinding> set_layout_bindings = {
	    vkb::initializers::descriptor_set_layout_binding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0)
	};

	VkDescriptorSetLayoutCreateInfo descriptor_layout_create_info =
	    vkb::initializers::descriptor_set_layout_create_info(set_layout_bindings.data(), static_cast<uint32_t>(set_layout_bindings.size()));

	VK_CHECK(vkCreateDescriptorSetLayout(get_device().get_handle(), &descriptor_layout_create_info, VK_NULL_HANDLE, &descriptor_set_layouts.model));

	VkPipelineLayoutCreateInfo pipeline_layout_create_info =
	    vkb::initializers::pipeline_layout_create_info(
	        &descriptor_set_layouts.model,
	        1);

	VK_CHECK(vkCreatePipelineLayout(get_device().get_handle(), &pipeline_layout_create_info, VK_NULL_HANDLE, &pipeline_layouts.model));

	/* Background */
	set_layout_bindings = {
	    vkb::initializers::descriptor_set_layout_binding(
	        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        VK_SHADER_STAGE_VERTEX_BIT,
	        0),
	    vkb::initializers::descriptor_set_layout_binding(
	        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	        VK_SHADER_STAGE_FRAGMENT_BIT,
	        1),
	};

	descriptor_layout_create_info.pBindings    = set_layout_bindings.data();
	descriptor_layout_create_info.bindingCount = static_cast<uint32_t>(set_layout_bindings.size());
	VK_CHECK(vkCreateDescriptorSetLayout(get_device().get_handle(), &descriptor_layout_create_info, nullptr, &descriptor_set_layouts.background));

	pipeline_layout_create_info.pSetLayouts            = &descriptor_set_layouts.background;
	pipeline_layout_create_info.setLayoutCount         = 1;
	pipeline_layout_create_info.pushConstantRangeCount = 0;
	pipeline_layout_create_info.pPushConstantRanges    = VK_NULL_HANDLE;
	VK_CHECK(vkCreatePipelineLayout(get_device().get_handle(), &pipeline_layout_create_info, nullptr, &pipeline_layouts.background));
}

void LogicOpDynamicState::create_descriptor_sets()
{
	VkDescriptorSetAllocateInfo alloc_info =
	    vkb::initializers::descriptor_set_allocate_info(
	        descriptor_pool,
	        &descriptor_set_layouts.model,
	        1);

	VK_CHECK(vkAllocateDescriptorSets(get_device().get_handle(), &alloc_info, &descriptor_sets.model));

	VkDescriptorBufferInfo matrix_model_buffer_descriptor = create_descriptor(*uniform_buffers.model);
	
	std::vector<VkWriteDescriptorSet> write_descriptor_sets = {
	    vkb::initializers::write_descriptor_set(
	        descriptor_sets.model,
	        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        0,
	        &matrix_model_buffer_descriptor)};

	vkUpdateDescriptorSets(get_device().get_handle(), static_cast<uint32_t>(write_descriptor_sets.size()), write_descriptor_sets.data(), 0, VK_NULL_HANDLE);

	/**/

	alloc_info =
	    vkb::initializers::descriptor_set_allocate_info(
	        descriptor_pool,
	        &descriptor_set_layouts.background,
	        1);

	VK_CHECK(vkAllocateDescriptorSets(get_device().get_handle(), &alloc_info, &descriptor_sets.background));

	VkDescriptorBufferInfo matrix_background_buffer_descriptor = create_descriptor(*uniform_buffers.background);
	VkDescriptorImageInfo  background_image_descriptor         = create_descriptor(textures.envmap);

	write_descriptor_sets = {
	    vkb::initializers::write_descriptor_set(
	        descriptor_sets.background,
	        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
	        0,
	        &matrix_background_buffer_descriptor),
	    vkb::initializers::write_descriptor_set(
	        descriptor_sets.background,
	        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
	        1,
	        &background_image_descriptor)};

	vkUpdateDescriptorSets(get_device().get_handle(), static_cast<uint32_t>(write_descriptor_sets.size()), write_descriptor_sets.data(), 0, NULL);
}

/**
 * @fn void LogicOpDynamicState::draw_from_scene(VkCommandBuffer command_buffer, std::vector<SceneNode> const &scene_node)
 * @brief Drawing all objects included to scene that is passed as argument of this function
 */
void LogicOpDynamicState::draw_from_scene(VkCommandBuffer command_buffer, std::vector<SceneNode> const &scene_node)
{
	for (int i = 0; i < scene_node.size(); i++)
	{
		const auto &vertex_buffer_pos    = scene_node[i].sub_mesh->vertex_buffers.at("position");
		const auto &vertex_buffer_normal = scene_node[i].sub_mesh->vertex_buffers.at("normal");
		auto &      index_buffer         = scene_node[i].sub_mesh->index_buffer;

		//if (scene_node.at(i).name != "Geosphere")
		//{
		//	vkCmdSetDepthBiasEnableEXT(command_buffer, gui_settings.objects[i].depth_bias);
		//	vkCmdSetRasterizerDiscardEnableEXT(command_buffer, gui_settings.objects[i].rasterizer_discard);
		//}

		/* Pass data for the current node via push commands */
		auto node_material            = dynamic_cast<const vkb::sg::PBRMaterial *>(scene_node[i].sub_mesh->get_material());
		push_const_block.model_matrix = scene_node[i].node->get_transform().get_world_matrix();
		//if (gui_settings.selection_active && i == gui_settings.selected_obj)
		//{
		//	push_const_block.color = get_changed_alpha(node_material);
		//}
		//else
		//{
		push_const_block.color = node_material->base_color_factor;
		//}
		vkCmdPushConstants(command_buffer, pipeline_layouts.model, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_const_block), &push_const_block);

		VkDeviceSize offsets[1] = {0};
		vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffer_pos.get(), offsets);
		vkCmdBindVertexBuffers(command_buffer, 1, 1, vertex_buffer_normal.get(), offsets);
		vkCmdBindIndexBuffer(command_buffer, index_buffer->get_handle(), 0, scene_node[i].sub_mesh->index_type);

		vkCmdDrawIndexed(command_buffer, scene_node[i].sub_mesh->vertex_indices, 1, 0, 0, 0);
	}
	vkCmdSetDepthBiasEnableEXT(command_buffer, VK_FALSE);
	vkCmdSetRasterizerDiscardEnableEXT(command_buffer, VK_FALSE);
}

void LogicOpDynamicState::model_data_creation()
{
// Probably to remove
}

//void LogicOpDynamicState::draw_created_model(VkCommandBuffer commandBuffer)
//{
	// Probably to remove
	// 
	//VkDeviceSize offsets[1] = {0};
	//vkCmdBindVertexBuffers(commandBuffer, 0, 1, cube.vertices->get(), offsets);
	//vkCmdBindIndexBuffer(commandBuffer, cube.indices->get_handle(), 0, VK_INDEX_TYPE_UINT32);
	//vkCmdDrawIndexed(commandBuffer, cube.index_count, 1, 0, 0, 0);
//}

/**
 * @fn void LogicOpDynamicState::on_update_ui_overlay(vkb::Drawer &drawer)
 * @brief Projecting GUI and transferring data between GUI and app
 */
void LogicOpDynamicState::on_update_ui_overlay(vkb::Drawer &drawer)
{
	std::vector<std::string> combo_box_items = {"Or",
	                                            "XOR",
	                                            "And"};
	if (drawer.header("Settings"))
	{
		if (drawer.combo_box("Logic operation", &gui_settings.selectd_operation, combo_box_items))
		{
			update_uniform_buffers();
		}
	}
}
std::unique_ptr<vkb::VulkanSample> create_logic_op_dynamic_state()
{
	return std::make_unique<LogicOpDynamicState>();
}
