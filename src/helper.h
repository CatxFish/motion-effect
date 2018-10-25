/*
*	motion-effect, an OBS-Studio filter plugin for animating sources using
*	transform manipulation on the scene.
*	Copyright(C) <2018>  <CatxFish>
*
*	This program is free software; you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation; either version 2 of the License, or
*	(at your option) any later version.
*
*	This program is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License along
*	with this program; if not, write to the Free Software Foundation, Inc.,
*	51 Franklin Street, Fifth Floor, Boston, MA 02110 - 1301 USA.
*/

#pragma once

#include <obs-module.h>

obs_sceneitem_t* get_item(obs_source_t *context,const char *name);
obs_sceneitem_t* get_item_by_id(obs_source_t *context,int64_t id);
int64_t get_item_id(obs_source_t *context, const char *name);

bool cal_size(obs_sceneitem_t* item, float sx, float sy, int *width,
	int *height);

bool check_item_basesize(obs_sceneitem_t *item);

bool cal_scale(obs_sceneitem_t *item, float *sx, float *sy, int width,
	int height);

void set_item_scale(obs_sceneitem_t *item, int width, int height);

bool is_program_scene(obs_source_t *scene);

obs_hotkey_id register_hotkey(obs_source_t *context, obs_source_t *scene,
	const char *name, const char *text, obs_hotkey_func func, void *data);

void unregister_hotkey(obs_hotkey_id id);

bool same_transform_type(struct obs_transform_info *info_a,
	struct obs_transform_info *info_b);

void save_hotkey_config(obs_hotkey_id id, obs_data_t *settings,
	const char *name);

float bezier(float point[], float percent, int order);

void vec_linear(struct vec2 a, struct vec2 b, struct vec2 *result, float t);

void vec_bezier(struct vec2 a, struct vec2 b, struct vec2 c,
	struct vec2 *result, float t);

void crop_linear(struct obs_sceneitem_crop a, struct obs_sceneitem_crop b,
	struct obs_sceneitem_crop* result, float t);