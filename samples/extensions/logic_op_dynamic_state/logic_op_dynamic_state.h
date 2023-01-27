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

#pragma once

#include "api_vulkan_sample.h"
//#include "rendering/render_pipeline.h"
//#include "scene_graph/components/camera.h"

class LogicOpDynamicState : public ApiVulkanSample
{
  public:
	//struct UBOVS
	//{
	//	glm::mat4 projection;
	//	glm::mat4 modelview;
	//	glm::mat4 background_modelview;
	//	//float     modelscale = 0.15f;
	//} ubo_vs;

	struct UBOBAS
	{
		glm::mat4 projection;
		glm::mat4 view;
		glm::vec4 ambientLightColor = glm::vec4(1.f, 1.f, 1.f, 0.1f);
		glm::vec4 lightPosition     = glm::vec4(-3.0f, -8.0f, 6.0f, -1.0f);
		glm::vec4 lightColor        = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		float     lightIntensity    = 50.0f;
	} ubo_model;

	struct UBOBG
	{
		glm::mat4 projection;
		glm::mat4 background_modelview;

	} ubo_background;

	struct
	{
		VkDescriptorSetLayout model{VK_NULL_HANDLE};
		VkDescriptorSetLayout background{VK_NULL_HANDLE};
	} descriptor_set_layouts;

	struct
	{
		VkPipelineLayout model{VK_NULL_HANDLE};
		VkPipelineLayout background{VK_NULL_HANDLE};
	} pipeline_layouts;

	struct
	{
		VkDescriptorSet model{VK_NULL_HANDLE};
		VkDescriptorSet background{VK_NULL_HANDLE};
	} descriptor_sets;

	struct
	{
		VkPipeline model{VK_NULL_HANDLE};
		VkPipeline background{VK_NULL_HANDLE};
	} pipeline;

	struct
	{
		std::unique_ptr<vkb::core::Buffer> model;
		std::unique_ptr<vkb::core::Buffer> background;
	} uniform_buffers;

	std::unique_ptr<vkb::sg::SubMesh> background_model;
	std::unique_ptr<vkb::sg::SubMesh>  model;
	std::unique_ptr<vkb::core::Buffer> ubo;

	struct
	{
		Texture envmap;
	} textures;

	struct
	{
		glm::mat4 model_matrix;
		glm::vec4 color;
	} push_const_block;

	struct SceneNode
	{
		std::string       name;
		vkb::sg::Node *   node;
		vkb::sg::SubMesh *sub_mesh;
	};
	std::vector<SceneNode> scene_elements_model;

	struct
	{
		int selectd_operation = 0;
		bool time_tick = false;
	} gui_settings;

	LogicOpDynamicState();
	virtual ~LogicOpDynamicState();

	virtual bool prepare(vkb::Platform &platform) override;
	virtual void render(float delta_time) override;
	virtual void build_command_buffers() override;
	virtual void request_gpu_features(vkb::PhysicalDevice &gpu) override;
	//virtual void update(float delta_time) override;

	void prepare_uniform_buffers();
	void update_uniform_buffers();
	void create_pipeline();
	void draw();

	void load_assets();
	void create_descriptor_pool();
	void setup_descriptor_set_layout();
	void create_descriptor_sets();

	void model_data_creation();
	//void draw_created_model(VkCommandBuffer commandBuffer);
	void on_update_ui_overlay(vkb::Drawer &drawer);

	//?
	void draw_from_scene(VkCommandBuffer command_buffer, std::vector<SceneNode> const &scene_node);

  protected:
	virtual void create_render_context(vkb::Platform &platform) override;
};

std::unique_ptr<vkb::VulkanSample> create_logic_op_dynamic_state();
