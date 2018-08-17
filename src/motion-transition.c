#include "obs-module.h"
#include <obs-scene.h>

enum variation_type {
	VARIATION_MOVING = 0,
	VARIATION_ZOOMOUT = 1,
	VARIATION_ZOOMIN = 2
};

typedef struct moving_item moving_item_t;
typedef struct list_info list_info_t;
typedef struct transition_data transition_data_t;

struct moving_item {
	obs_sceneitem_t     *item;
	enum variation_type type;
	struct vec2         start_pos;
	struct vec2         start_scale;
	struct vec2         end_pos;
	struct vec2         end_scale;
	moving_item_t       *next;
};



struct list_info {
	obs_scene_t        *scene;
	moving_item_t      *last_item;
	moving_item_t      *first_item;
	bool               trantion_out;
};


struct transition_data {
	obs_source_t        *source;
	obs_source_t        *scene_a_source;
	obs_source_t        *scene_b_source;
	obs_scene_t         *scene_a;
	obs_scene_t         *scene_b;
	moving_item_t       *out_item;
	moving_item_t       *in_item;
	bool                start_init;
	bool                scene_transition;
	bool                transitioning;
};

static bool append_item_list(obs_scene_t *scene, obs_sceneitem_t *item, void *data)
{
	list_info_t* tr = data;
	struct obs_transform_info src_info;
	struct obs_transform_info dst_info;
	obs_source_t* item_src = obs_sceneitem_get_source(item);
	obs_sceneitem_t *dst_item;
	moving_item_t *next = bzalloc(sizeof(*next));

	obs_sceneitem_get_info(item, &src_info);
	dst_item = obs_scene_find_source(tr->scene, obs_source_get_name(item_src));

	if (dst_item) {
		obs_sceneitem_get_info(dst_item, &dst_info);
		next->type = VARIATION_MOVING;
	} else {
		float w = obs_source_get_base_width(item_src) * src_info.scale.x;
		float h = obs_source_get_base_height(item_src) * src_info.scale.y;
		dst_info.pos.x = src_info.pos.x + w / 2;
		dst_info.pos.y = src_info.pos.y + h / 2;
		dst_info.scale.x = 0;
		dst_info.scale.y = 0;
		next->type = tr->trantion_out ? VARIATION_ZOOMOUT : VARIATION_ZOOMIN;
	}

	next->item = item;
	if (tr->trantion_out) {
		vec2_copy(&next->start_pos, &src_info.pos);
		vec2_copy(&next->start_scale, &src_info.scale);
		vec2_copy(&next->end_pos, &dst_info.pos);
		vec2_copy(&next->end_scale, &dst_info.scale);
	}
	else {
		vec2_copy(&next->start_pos, &dst_info.pos);
		vec2_copy(&next->start_scale, &dst_info.scale);
		vec2_copy(&next->end_pos, &src_info.pos);
		vec2_copy(&next->end_scale, &src_info.scale);
	}

	if (tr->last_item)
		tr->last_item->next = next;
	else
		tr->first_item = next;

	tr->last_item = next;

	return true;
}

static void create_item_list(transition_data_t* tr)
{
	list_info_t info;

	memset(&info, 0, sizeof(info));
	info.scene = tr->scene_b;
	info.last_item = tr->out_item;
	info.trantion_out = true;
	obs_scene_enum_items(tr->scene_a, append_item_list, &info);
	tr->out_item = info.first_item;

	memset(&info, 0, sizeof(info));
	info.scene = tr->scene_a;
	info.last_item = tr->in_item;
	info.trantion_out = false;
	obs_scene_enum_items(tr->scene_b, append_item_list, &info);
	tr->in_item = info.first_item;
}

static void release_item_list(moving_item_t *items)
{
	moving_item_t *next;
	while (items) {
		next = items->next;
		bfree(items);
		items = next;
	}
}

static void update_item_information(moving_item_t *items, float time)
{
	struct vec2 pos;
	struct vec2 scale;
	float t;

	while(items) {

		if (items->type == VARIATION_ZOOMOUT)
			t = time * 2;
		else if (items->type == VARIATION_ZOOMIN)
			t = time * 2 - 1.0f;
		else
			t = time;

		pos.x = (1.0f - t) * items->start_pos.x + t * items->end_pos.x;
		pos.y = (1.0f - t) * items->start_pos.y + t * items->end_pos.y;
		scale.x = (1.0f - t) * items->start_scale.x + t * items->end_scale.x;
		scale.y = (1.0f - t) * items->start_scale.y + t * items->end_scale.y;
		obs_sceneitem_set_pos(items->item, &pos);
		obs_sceneitem_set_scale(items->item, &scale);	
		items = items->next;
	}
}

static void motion_transition_update(void *data, obs_data_t *settings)
{


}

static void motion_transition_start(void *data)
{
	transition_data_t *tr = data;
	tr->transitioning = true;
	tr->start_init = true;
}

static void motion_transition_stop(void *data)
{
	transition_data_t *tr = data;
	release_item_list(tr->in_item);
	release_item_list(tr->out_item);
	tr->in_item = NULL;
	tr->out_item = NULL;
	
	if (tr->scene_transition) {
		obs_source_remove_active_child(tr->source, tr->scene_a_source);
		obs_source_remove_active_child(tr->source, tr->scene_b_source);
		obs_scene_release(tr->scene_a);
		obs_scene_release(tr->scene_b);
		tr->scene_a = NULL;
		tr->scene_b = NULL;
		tr->scene_a_source = NULL;
		tr->scene_b_source = NULL;
	}

	tr->transitioning = false;
}

static obs_properties_t *motion_transition_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();
	return props;
}

static void motion_transition_tick(void *data, float seconds)
{


}

static void motion_transition_video_render(void *data, gs_effect_t *effect)
{
	transition_data_t *tr = data;

	float t = obs_transition_get_time(tr->source);

	if (t <= 0.0f) {
		obs_transition_video_render_direct(tr->source, OBS_TRANSITION_SOURCE_A);
		return;
	} else if (t >= 1.0f) {
		obs_transition_video_render_direct(tr->source, OBS_TRANSITION_SOURCE_B);
		return;
	}

	if (tr->start_init) {

		obs_source_t *source_a = obs_transition_get_source(tr->source,
			OBS_TRANSITION_SOURCE_A);
		obs_scene_t *scene_a = obs_scene_from_source(source_a);

		obs_source_t *source_b = obs_transition_get_source(tr->source,
			OBS_TRANSITION_SOURCE_B);
		obs_scene_t *scene_b = obs_scene_from_source(source_b);
		tr->scene_transition = scene_a && scene_b;

		if (tr->scene_transition) {

			tr->scene_a = obs_scene_duplicate(scene_a, "motion-transition-a",
				OBS_SCENE_DUP_PRIVATE_REFS);
			obs_source_add_active_child(tr->source, source_a);

			tr->scene_a_source = obs_scene_get_source(tr->scene_a);

			tr->scene_b = obs_scene_duplicate(scene_b, "motion-transition-b",
				OBS_SCENE_DUP_PRIVATE_REFS);
			obs_source_add_active_child(tr->source, source_b);

			tr->scene_b_source = obs_scene_get_source(tr->scene_b);

			create_item_list(tr);
		}
		obs_source_release(source_a);
		obs_source_release(source_b);
		tr->start_init = false;
	}

	if (tr->scene_transition) {
		if (t <= 0.5) {
			update_item_information(tr->out_item, t);
			obs_source_video_render(tr->scene_a_source);
		} else {
			update_item_information(tr->in_item, t);
			obs_source_video_render(tr->scene_b_source);
		}
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
	return obs_transition_audio_render(tr->source, ts_out,
		audio, mixers, channels, sample_rate, mix_a, mix_b);
}

static void motion_enum_all_sources(void *data,
	obs_source_enum_proc_t enum_callback, void *param)
{
	transition_data_t* tr = data;
	if (tr->scene_a_source)
		enum_callback(tr->source, tr->scene_a_source, param);

	if (tr->scene_b_source)
		enum_callback(tr->source, tr->scene_b_source, param);

}


static void motion_enum_active_sources(void *data,
	obs_source_enum_proc_t enum_callback, void *param)
{
	transition_data_t* tr = data;
	if (tr->scene_a_source && tr->transitioning)
		enum_callback(tr->source, tr->scene_a_source, param);

	if (tr->scene_b_source && tr->transitioning)
		enum_callback(tr->source, tr->scene_b_source, param);
}

static void *motion_transition_create(obs_data_t *settings, obs_source_t *source)
{
	transition_data_t *tr = bzalloc(sizeof(*tr));
	tr->source = source;
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