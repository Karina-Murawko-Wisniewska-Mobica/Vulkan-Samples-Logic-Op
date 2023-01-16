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

#include "common/vk_common.h"
#include "gltf_loader.h"
#include "gui.h"
#include "platform/filesystem.h"
#include "platform/platform.h"
#include "rendering/subpasses/forward_subpass.h"
#include "stats/stats.h"

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
		// TODO update after defining pipline/pipline_layouts/descriptor_set_layouts
		// vkDestroyPipeline(get_device().get_handle(), pipeline.baseline, VK_NULL_HANDLE);
		// vkDestroyPipeline(get_device().get_handle(), pipeline.background, VK_NULL_HANDLE);
		// vkDestroyPipelineLayout(get_device().get_handle(), pipeline_layouts.baseline, VK_NULL_HANDLE);
		// vkDestroyPipelineLayout(get_device().get_handle(), pipeline_layouts.background, VK_NULL_HANDLE);
		// vkDestroyDescriptorSetLayout(get_device().get_handle(), descriptor_set_layouts.baseline, VK_NULL_HANDLE);
		// vkDestroyDescriptorSetLayout(get_device().get_handle(), descriptor_set_layouts.background, VK_NULL_HANDLE);
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

	// Load a scene from the assets folder
	load_scene("scenes/sponza/Sponza01.gltf");

	// Attach a move script to the camera component in the scene
	auto &camera_node = vkb::add_free_camera(*scene, "main_camera", get_render_context().get_surface_extent());
	auto  camera      = &camera_node.get_component<vkb::sg::Camera>();

	// Example Scene Render Pipeline
	vkb::ShaderSource vert_shader("base.vert");
	vkb::ShaderSource frag_shader("base.frag");
	auto              scene_subpass   = std::make_unique<vkb::ForwardSubpass>(get_render_context(), std::move(vert_shader), std::move(frag_shader), *scene, *camera);
	auto              render_pipeline = vkb::RenderPipeline();
	render_pipeline.add_subpass(std::move(scene_subpass));
	set_render_pipeline(std::move(render_pipeline));

	// Add a GUI with the stats you want to monitor
	// TODO uncomment: currently it crashes
	// stats->request_stats({/*stats you require*/});
	// gui = std::make_unique<vkb::Gui>(*this, platform.get_window(), stats.get());

	return true;
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
 */
void LogicOpDynamicState::build_command_buffers()
{
	// TODO
}

/**
 * @fn void LogicOpDynamicState::request_gpu_features(vkb::PhysicalDevice &gpu)
 * @brief Enabling features related to Vulkan extensions
 */
void LogicOpDynamicState::request_gpu_features(vkb::PhysicalDevice &gpu)
{
	/* Enable extension features required by this sample
	   These are passed to device creation via a pNext structure chain */
	auto &requested_extended_dynamic_state2_features                                   = gpu.request_extension_features<VkPhysicalDeviceExtendedDynamicState2FeaturesEXT>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_2_FEATURES_EXT);
	requested_extended_dynamic_state2_features.extendedDynamicState2                   = VK_TRUE;
	requested_extended_dynamic_state2_features.extendedDynamicState2LogicOp            = VK_TRUE;

	auto &requested_extended_dynamic_state_feature                = gpu.request_extension_features<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT);
	requested_extended_dynamic_state_feature.extendedDynamicState = VK_TRUE;

	if (gpu.get_features().samplerAnisotropy)
	{
		gpu.get_mutable_requested_features().samplerAnisotropy = VK_TRUE;
	}
}

void LogicOpDynamicState::prepare_uniform_buffers()
{}

void LogicOpDynamicState::update_uniform_buffers()
{}

void LogicOpDynamicState::create_pipeline()
{}

void LogicOpDynamicState::draw()
{}

void LogicOpDynamicState::load_assets()
{}

void LogicOpDynamicState::create_descriptor_pool()
{}

void LogicOpDynamicState::setup_descriptor_set_layout()
{}

void LogicOpDynamicState::create_descriptor_sets()
{}

void LogicOpDynamicState::model_data_creation()
{}

void LogicOpDynamicState::draw_created_model(VkCommandBuffer commandBuffer)
{}

/**
 * @fn void LogicOpDynamicState::on_update_ui_overlay(vkb::Drawer &drawer)
 * @brief Projecting GUI and transferring data between GUI and app
 */
void LogicOpDynamicState::on_update_ui_overlay(vkb::Drawer &drawer)
{
}
std::unique_ptr<vkb::VulkanSample> create_logic_op_dynamic_state()
{
	return std::make_unique<LogicOpDynamicState>();
}
