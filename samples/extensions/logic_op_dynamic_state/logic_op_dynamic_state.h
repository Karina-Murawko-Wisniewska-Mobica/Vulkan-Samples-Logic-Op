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
#include "rendering/render_pipeline.h"
#include "scene_graph/components/camera.h"

class LogicOpDynamicState : public ApiVulkanSample
{
  public:
	LogicOpDynamicState();
	virtual ~LogicOpDynamicState();

	virtual bool prepare(vkb::Platform &platform) override;
	virtual void render(float delta_time) override;
	virtual void build_command_buffers() override;
	virtual void request_gpu_features(vkb::PhysicalDevice &gpu) override;

	void prepare_uniform_buffers();
	void update_uniform_buffers();
	void create_pipeline();
	void draw();

	void load_assets();
	void create_descriptor_pool();
	void setup_descriptor_set_layout();
	void create_descriptor_sets();

	void model_data_creation();
	void draw_created_model(VkCommandBuffer commandBuffer);
	void on_update_ui_overlay(vkb::Drawer &drawer);
};

std::unique_ptr<vkb::VulkanSample> create_logic_op_dynamic_state();
