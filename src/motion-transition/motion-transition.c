/*
*	motion-effect, an OBS-Studio plugin for animating sources
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


#include "obs-module.h"
#include "../helper.h"
#include <obs-scene.h>

enum variation_type {
	VARIATION_MOTION = 0,
	VARIATION_ZOOMOUT = 1,
	VARIATION_ZOOMIN = 2
};


#define S_BEZIER_X        "bezier_x"
#define S_BEZIER_Y        "bezier_y"

#define T_(v)             obs_module_text(v)
#define T_BEZIER_X        T_("Acceleration.X")
#define T_BEZIER_Y        T_("Acceleration.Y")


typedef struct moving_item moving_item_t;
typedef struct list_info list_info_t;
typedef struct transition_data transition_data_t;

struct moving_item {
	obs_sceneitem_t           *item;
	enum variation_type       type;
	struct obs_transform_info start_info;
	struct obs_transform_info end_info;
	struct obs_sceneitem_crop start_crop;
	struct obs_sceneitem_crop end_crop;
	struct vec2               control_pos;
	moving_item_t             *next;
};

struct list_info {
	obs_scene_t        *scene;
	obs_source_t       *source;
	moving_item_t      *last_item;
	moving_item_t      *first_item;
};

struct transition_data {
	obs_source_t        *context;
	list_info_t         out_list;
	list_info_t         in_list;
	float               acc_x;
	float               acc_y;
	bool                start_init;
	bool                scene_transition;
	bool                transitioning;
};

static bool append_item_list(obs_scene_t *scene, obs_sceneitem_t *item_a, void *data)
{
	transition_data_t *tr = data;
	bool transition_out = tr->out_list.scene == scene;
	bool transform_variation = false;
	list_info_t *list, *list_cmp;
	struct obs_transform_info *info_a, *info_b;
	struct obs_sceneitem_crop *crop_a, *crop_b;
	obs_source_t *source_a = obs_sceneitem_get_source(item_a);
	obs_sceneitem_t *item_b;
	moving_item_t *next = bzalloc(sizeof(*next));

	if (transition_out) {
		list = &tr->out_list;
		list_cmp = &tr->in_list;
		info_a = &next->start_info;
		info_b = &next->end_info;
		crop_a = &next->start_crop;
		crop_b = &next->end_crop;
	} else {
		list = &tr->in_list;
		list_cmp = &tr->out_list;
		info_a = &next->end_info;
		info_b = &next->start_info;
		crop_a = &next->end_crop;
		crop_b = &next->start_crop;
	}

	obs_sceneitem_get_info(item_a, info_a);
	obs_sceneitem_get_crop(item_a, crop_a);
	item_b = obs_scene_find_source(list_cmp->scene, 
		obs_source_get_name(source_a));


	if (item_b) {
		obs_sceneitem_get_info(item_b, info_b);
		obs_sceneitem_get_crop(item_b, crop_b);
		transform_variation = same_transform_type(info_a, info_b) && (item_a->user_visible==item_b->user_visible);
	}

	if (transform_variation) {
		float t = transition_out ? tr->acc_x : 1 - tr->acc_x;
		float f = transition_out ? tr->acc_y : 1 - tr->acc_y;
		next->control_pos.x = (1 - t) * info_a->pos.x + t * info_b->pos.x;
		next->control_pos.y = (1 - f) * info_a->pos.y + f * info_b->pos.y;
		next->type = VARIATION_MOTION;
	} else {
		float w = obs_source_get_base_width(source_a) * info_a->scale.x;
		float h = obs_source_get_base_height(source_a) * info_a->scale.y;
		info_b->pos.x = info_a->pos.x + w / 2;
		info_b->pos.y = info_a->pos.y + h / 2;
		info_b->scale.x = 0;
		info_b->scale.y = 0;
		next->type = transition_out ? VARIATION_ZOOMOUT : VARIATION_ZOOMIN;
	}

	next->item = item_a;

	if (list->last_item)
		list->last_item->next = next;
	else
		list->first_item = next;

	list->last_item = next;

	return true;
}

static void create_item_list(transition_data_t* tr)
{
	obs_scene_enum_items(tr->out_list.scene, append_item_list, tr);
	obs_scene_enum_items(tr->in_list.scene, append_item_list, tr);
}

static void release_item_list(list_info_t *list)
{
	moving_item_t *mv = list->first_item;
	moving_item_t *next;
	while (mv) {
		next = mv->next;
		bfree(mv);
		mv = next;
	}

	memset(list, 0, sizeof(list_info_t));
}

static void update_item_information(moving_item_t *mv, float time)
{
	struct vec2 pos;
	struct vec2 scale;
	struct vec2 bounds;
	struct obs_sceneitem_crop crop;
	float rot;
	float t;

	while(mv) {

		if (mv->type == VARIATION_MOTION) {
			t = time;
			vec_bezier(mv->start_info.pos, mv->control_pos, mv->end_info.pos, 
				&pos, t);
			vec_linear(mv->start_info.bounds, mv->end_info.bounds, &bounds, t);
			crop_linear(mv->start_crop, mv->end_crop, &crop, t);
			rot = (1.0f - t) * mv->start_info.rot + t * mv->end_info.rot;
			obs_sceneitem_set_bounds(mv->item, &bounds);
			obs_sceneitem_set_crop(mv->item, &crop);
			obs_sceneitem_set_rot(mv->item, rot);

		} else if (mv->type == VARIATION_ZOOMIN) {
			t = time * 2 - 1.0f;
			vec_linear(mv->start_info.pos, mv->end_info.pos, &pos, t);
		} else {
			t = time * 2;
			vec_linear(mv->start_info.pos, mv->end_info.pos, &pos, t);
		}
		
		vec_linear(mv->start_info.scale, mv->end_info.scale, &scale, t);

		obs_sceneitem_set_pos(mv->item, &pos);
		obs_sceneitem_set_scale(mv->item, &scale);	
		mv = mv->next;
	}
}

static void motion_transition_update(void *data, obs_data_t *settings)
{
	transition_data_t *tr = data;
	float x = (float)obs_data_get_double(settings, S_BEZIER_X);
	float y = (float)obs_data_get_double(settings, S_BEZIER_Y);
	
	tr->acc_x = - x + 0.5f;
	tr->acc_y = - y + 0.5f;
}

static void motion_transition_start(void *data)
{
	transition_data_t *tr = data;
	tr->start_init = true;
}

static void motion_transition_stop(void *data)
{
	transition_data_t *tr = data;
	
	obs_source_remove_active_child(tr->context, tr->in_list.source);
	obs_source_remove_active_child(tr->context, tr->out_list.source);
	obs_scene_release(tr->in_list.scene);
	obs_scene_release(tr->out_list.scene);	
	release_item_list(&tr->in_list);
	release_item_list(&tr->out_list);
	tr->transitioning = false;
}

static obs_properties_t *motion_transition_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();
	obs_properties_add_float_slider(props, S_BEZIER_X, T_BEZIER_X, -0.5, 0.5,
		0.01);
	obs_properties_add_float_slider(props, S_BEZIER_Y, T_BEZIER_Y, -0.5, 0.5,
		0.01);
	return props;
}

static void motion_transition_video_render(void *data, gs_effect_t *effect)
{
	transition_data_t *tr = data;

	float t = obs_transition_get_time(tr->context);

	if (tr->start_init) {

		if (tr->transitioning)
			motion_transition_stop(tr);

		obs_source_t *source_a = obs_transition_get_source(tr->context,
			OBS_TRANSITION_SOURCE_A);
		obs_scene_t *scene_a = obs_scene_from_source(source_a);

		obs_source_t *source_b = obs_transition_get_source(tr->context,
			OBS_TRANSITION_SOURCE_B);
		obs_scene_t *scene_b = obs_scene_from_source(source_b);
		tr->scene_transition = scene_a && scene_b;

		if (tr->scene_transition) {

			tr->out_list.scene = obs_scene_duplicate(scene_a,
				"motion-transition-a", OBS_SCENE_DUP_PRIVATE_REFS);
			tr->out_list.source = obs_scene_get_source(tr->out_list.scene);
			obs_source_add_active_child(tr->context, tr->out_list.source);

			tr->in_list.scene = obs_scene_duplicate(scene_b,
				"motion-transition-b", OBS_SCENE_DUP_PRIVATE_REFS);
			tr->in_list.source = obs_scene_get_source(tr->in_list.scene);
			obs_source_add_active_child(tr->context, tr->in_list.source);

			create_item_list(tr);
			tr->transitioning = true;
		}
		obs_source_release(source_a);
		obs_source_release(source_b);
		tr->start_init = false;
	}

	if (t > 0.0f && t < 1.0f && tr->scene_transition) {
		if (t <= 0.5) {
			update_item_information(tr->out_list.first_item, t);
			obs_source_video_render(tr->out_list.source);
		} else {
			update_item_information(tr->in_list.first_item, t);
			obs_source_video_render(tr->in_list.source);
		}
	} else if (t <= 0.5f ) {
		obs_transition_video_render_direct(tr->context,
			OBS_TRANSITION_SOURCE_A);
	} else {
		obs_transition_video_render_direct(tr->context,
				OBS_TRANSITION_SOURCE_B);
	}

}

static float mix_a(void *data, float t)
{
	UNUSED_PARAMETER(data);
	return 1.0f - t;
}

static float mix_b(void *data, float t)
{
	UNUSED_PARAMETER(data);
	return t;
}


static bool motion_transition_audio_render(void *data, uint64_t *ts_out,
	struct obs_source_audio_mix *audio, uint32_t mixers,
	size_t channels, size_t sample_rate)
{
	transition_data_t *tr = data;
	return obs_transition_audio_render(tr->context, ts_out,
		audio, mixers, channels, sample_rate, mix_a, mix_b);
}

static void motion_enum_all_sources(void *data,
	obs_source_enum_proc_t enum_callback, void *param)
{
	transition_data_t* tr = data;
	if (tr->out_list.source)
		enum_callback(tr->context, tr->out_list.source, param);

	if (tr->in_list.source)
		enum_callback(tr->context, tr->in_list.source, param);

}


static void motion_enum_active_sources(void *data,
	obs_source_enum_proc_t enum_callback, void *param)
{
	transition_data_t* tr = data;
	if (tr->out_list.source && tr->transitioning)
		enum_callback(tr->context, tr->out_list.source, param);

	if (tr->in_list.source && tr->transitioning)
		enum_callback(tr->context, tr->in_list.source, param);
}

static void *motion_transition_create(obs_data_t *settings, obs_source_t *context)
{
	transition_data_t *tr = bzalloc(sizeof(*tr));
	tr->context = context;
	UNUSED_PARAMETER(settings);
	return tr;
}


static void motion_transition_destroy(void *data)
{
	transition_data_t *tr = data;
	bfree(tr);
}


static const char *motion_transition_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("Motion");
}

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("motion-transition", "en-US")

struct obs_source_info motion_transition = {
	.id = "motion-transition",
	.type = OBS_SOURCE_TYPE_TRANSITION,
	.get_name = motion_transition_get_name,
	.create = motion_transition_create,
	.destroy = motion_transition_destroy,
	.update = motion_transition_update,
	.video_render = motion_transition_video_render,
	.audio_render = motion_transition_audio_render,
	.get_properties = motion_transition_properties,
	.enum_active_sources = motion_enum_active_sources,
	.enum_all_sources = motion_enum_all_sources,
	.transition_start = motion_transition_start,
	.transition_stop = motion_transition_stop
};

bool obs_module_load(void) {
	obs_register_source(&motion_transition);
	return true;
}