/*
*	motion-effect, an OBS-Studio plugin for animating sources using
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

#include "helper.h"
#include <obs-scene.h>
#include <util/dstr.h>


obs_sceneitem_t *get_item(obs_source_t *context,
	const char *name)
{
	obs_source_t *source = obs_filter_get_parent(context);
	obs_scene_t *scene = obs_scene_from_source(source);
	return obs_scene_find_source(scene, name);
}

obs_sceneitem_t *get_item_by_id(obs_source_t* context, 
	int64_t id)
{
	obs_source_t *source = obs_filter_get_parent(context);
	obs_scene_t *scene = obs_scene_from_source(source);
	obs_sceneitem_t* item = obs_scene_find_sceneitem_by_id(scene, id);
	return item;
}

int64_t get_item_id(obs_source_t* context, const char* name)
{
	obs_sceneitem_t *item = get_item(context, name);
	return item ? obs_sceneitem_get_id(item) : -1;
}

bool cal_size(obs_sceneitem_t* item, float sx, float sy,
	int* width, int* height)
{
	obs_source_t *item_source = obs_sceneitem_get_source(item);
	int base_width = obs_source_get_base_width(item_source);
	int base_height = obs_source_get_base_height(item_source);

	*width = (int)(base_width * sx);
	*height = (int)(base_height * sy);

	return true;
}

bool check_item_basesize(obs_sceneitem_t* item)
{
	obs_source_t *item_source = obs_sceneitem_get_source(item);
	int base_width = obs_source_get_width(item_source);
	int base_height = obs_source_get_height(item_source);
	return base_width != 0 && base_height != 0;
}

bool cal_scale(obs_sceneitem_t* item, float* sx, float*sy,
	int width, int height)
{
	obs_source_t *item_source = obs_sceneitem_get_source(item);
	int base_width = obs_source_get_width(item_source);
	int base_height = obs_source_get_height(item_source);

	if (base_width == 0 || base_height == 0)
		return false;

	*sx = (float)width / base_width;
	*sy = (float)height / base_height;

	return true;
}

void set_item_scale(obs_sceneitem_t *item, int width, int height)
{
	struct vec2 scale;
	if (cal_scale(item, &scale.x, &scale.y, width, height))
		obs_sceneitem_set_scale(item, &scale);
}

/*
 * Workaround way to judge if is a program scene.
 * A program scene is a private source without name. 
*/

bool is_program_scene(obs_source_t *scene)
{
	if (!obs_scene_from_source(scene))
		return false;

	if (!scene->context.private)
		return false;

	if (obs_source_get_name(scene))
		return false;

	return true;
}

obs_hotkey_id register_hotkey(obs_source_t *context, obs_source_t *scene, 
	const char *name, const char *text, obs_hotkey_func func, void *data)
{
	const char *source_name = obs_source_get_name(context);
	const char *description;
	obs_data_t *settings = obs_source_get_settings(context);
	obs_data_array_t *save_array;
	struct dstr str = { 0 };
	obs_hotkey_id id;


	dstr_copy(&str, text);
	dstr_cat(&str, " [ %1 ] ");
	dstr_replace(&str, "%1", source_name);
	description = str.array;
	
	if (is_program_scene(scene)) {
		id = obs_hotkey_register_frontend(description, description, func,
			data);
	} else {
		id = obs_hotkey_register_source(scene, description, description,
			func, data);
	}

	save_array = obs_data_get_array(settings, name);
	obs_hotkey_load(id, save_array);
	obs_data_array_release(save_array);
	dstr_free(&str);
	obs_data_release(settings);

	return id;
}

void unregister_hotkey(obs_hotkey_id id)
{
	if (id != OBS_INVALID_HOTKEY_ID)
		obs_hotkey_unregister(id);

}

void save_hotkey_config(obs_hotkey_id id, obs_data_t *settings, 
	const char *name)
{
	obs_data_array_t* save_array = obs_hotkey_save(id);
	obs_data_set_array(settings, name, save_array);
	obs_data_array_release(save_array);
}

bool same_transform_type(struct obs_transform_info *info_a, 
	struct obs_transform_info *info_b)
{
	if (!info_a || !info_b)
		return false;
	
	return info_a->alignment == info_b->alignment &&
		info_a->bounds_type == info_b->bounds_type &&
		info_a->bounds_alignment == info_b->bounds_alignment;
}

float bezier(float point[], float coefficient, int order)
{
	float p = 1.0f - coefficient;
	float t = coefficient;

	if (order < 1)
		return point[0];
	else if (order == 1)
		return p * point[0] + t * point[1];
	else
		return p * bezier(point, t, order - 1) + 
		t * bezier(&point[1], t, order - 1);
}

void vec_linear(struct vec2 a, struct vec2 b,  struct vec2 *result ,float t)
{
	float x[2] = { a.x, b.x };
	float y[2] = { a.y, b.y };
	result->x = bezier(x, t, 1);
	result->y = bezier(y, t, 1);
}

void vec_bezier(struct vec2 a, struct vec2 b,  struct vec2 c ,
	struct vec2 *result, float t)
{
	float x[3] = { a.x,b.x,c.x };
	float y[3] = { a.y,b.y,c.y };
	result->x = bezier(x, t, 2);
	result->y = bezier(y, t, 2);
}

void crop_linear(struct obs_sceneitem_crop a, struct obs_sceneitem_crop b,
	struct obs_sceneitem_crop* result, float t)
{
	result->bottom = (1.0f - t) * a.bottom + t * b.bottom;
	result->left = (1.0f - t) * a.left + t * b.left;
	result->top = (1.0f - t) * a.top + t * b.top;
	result->right = (1.0f - t) * a.right + t * b.right;
}
