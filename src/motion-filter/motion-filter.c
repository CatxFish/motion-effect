/*
 *	motion-filter, an OBS-Studio filter plugin for animating sources using 
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

#include <obs-module.h>
#include <obs-hotkey.h>
#include <obs-scene.h>
#include <obs-frontend-api.h>
#include <util/dstr.h>
#include "../helper.h"

// Define property keys

enum {
	PATH_LINEAR = 0,
	PATH_QUADRATIC = 1,
	PATH_CUBIC = 2
};

enum {
	BEHAVIOR_NONE = 0,
	BEHAVIOR_ONE_WAY = 1,
	BEHAVIOR_ROUND_TRIP = 2,
	BEHAVIOR_SCENE_SWITCH =3
};

#define VARIATION_POSITION  (1<<0)
#define VARIATION_SIZE      (1<<1)

#define S_MOTION_END        "motion_end"
#define S_ORG_X             "org_x"
#define S_ORG_Y             "org_y"
#define S_ORG_W             "org_w"
#define S_ORG_H             "org_h"
#define S_START_X           "start_x"
#define S_START_Y           "start_y"
#define S_START_W           "start_w"
#define S_START_H           "start_h"
#define S_PATH_TYPE         "path_type"
#define S_START_SETTING     "start_setting"
#define S_CTRL_X            "ctrl_x"
#define S_CTRL_Y            "ctrl_y"
#define S_CTRL2_X           "ctrl2_x"
#define S_CTRL2_Y           "ctrl2_y"
#define S_DST_X             "dst_x"
#define S_DST_Y             "dst_y"
#define S_DST_W             "dst_w"
#define S_DST_H             "dst_h"
#define S_USE_DST_SCALE     "dst_use_scale"
#define S_DURATION          "duration"
#define S_ACCELERATION      "acceleration"
#define S_SOURCE            "source_id"
#define S_FORWARD           "forward"
#define S_BACKWARD          "backward"
#define S_DEST_GRAB_POS     "use_cur_src_pos"
#define S_MOTION_BEHAVIOR   "motion_behavior"
#define S_VARIATION_TYPE    "variation_type"
#define S_SCENE_NAME        "scene_name"

// Define property localisation tags
#define T_(v)               obs_module_text(v)
#define T_PATH_TYPE         T_("PathType")
#define T_PATH_LINEAR       T_("PathType.Linear")
#define T_PATH_QUADRATIC    T_("PathType.Quadratic")
#define T_PATH_CUBIC        T_("PathType.Cubic")
#define T_START_SETTING     T_("Start.Setting")
#define T_START_X           T_("Start.X")
#define T_START_Y           T_("Start.Y")
#define T_START_W           T_("Start.W")
#define T_START_H           T_("Start.H")
#define T_CTRL_X            T_("ControlPoint.X")
#define T_CTRL_Y            T_("ControlPoint.Y")
#define T_CTRL2_X           T_("ControlPoint2.X")
#define T_CTRL2_Y           T_("ControlPoint2.Y")
#define T_DST_X             T_("Destination.X")
#define T_DST_Y             T_("Destination.Y")
#define T_DST_W             T_("Destination.W")
#define T_DST_H             T_("Destination.H")
#define T_DURATION          T_("Duration")
#define T_ACCELERATION      T_("Acceleration")
#define T_SOURCE            T_("SourceName")
#define T_FORWARD           T_("Forward")
#define T_BACKWARD          T_("Backward")
#define T_DISABLED          T_("Disabled")
#define T_VARIATION_TYPE    T_("VariationType")
#define T_VARIATION_POS     T_("VariationType.Position")
#define T_VARIATION_SIZE    T_("VariationType.Size")
#define T_VARIATION_BOTH    T_("VariationType.PositionAndSize")
#define T_DEST_GRAB_POS     T_("DestinationGrabPosition")
#define T_MOTION_BEHAVIOR   T_("Behavior")
#define T_HOTKEY_ONE_WAY    T_("Behavior.OneWay")
#define T_HOTKEY_ROUND_TRIP T_("Behavior.RoundTrip")
#define T_SCENE_SWITCH      T_("Behavior.SceneSwitch")

typedef struct variation_data variation_data_t;
typedef struct motion_filter_data motion_filter_data_t;

struct variation_data {
	float               point_x[4];
	float               point_y[4];
	float               scale_x[2];
	float               scale_y[2];
	float               coeff[3];
	struct vec2         scale;
	struct vec2         position;	
	float               elapsed_time;
	bool                coeff_varaite;
};

struct motion_filter_data {
	obs_source_t        *context;
	obs_scene_t         *scene;
	obs_sceneitem_t     *item;
	obs_hotkey_id       hotkey_id_f;
	obs_hotkey_id       hotkey_id_b;
	variation_data_t    variation;
	bool                initialize;
	bool                restart_backward;
	bool                motion_start;
	bool                motion_end;
	bool                use_start_position;
	bool                use_start_scale;
	bool                change_position;
	bool                change_size;
	int                 motion_behavior;
	int                 path_type;
	int                 org_width;
	int                 org_height;
	int                 dst_width;
	int                 dst_height;
	struct vec2         org_pos;
	struct vec2         ctrl_pos;
	struct vec2         ctrl2_pos;
	struct vec2         dst_pos;
	float               duration;
	float               acceleration;
	char                *item_name;
	int64_t             item_id;
};

inline bool is_reverse(motion_filter_data_t *filter)
{
	return filter->motion_end && 
		filter->motion_behavior == BEHAVIOR_ROUND_TRIP;
}

inline const char* get_scene_name(motion_filter_data_t *filter)
{
	obs_source_t* scene = obs_filter_get_parent(filter->context);
	return obs_source_get_name(scene);
}

static void update_variation_data(motion_filter_data_t *filter)
{
	variation_data_t *var = &filter->variation;

	if (!check_item_basesize(filter->item))
		return ;

	if (!is_reverse(filter)) {
		struct obs_transform_info info;
		obs_sceneitem_get_info(filter->item, &info);
		if (!filter->use_start_position) {
			var->point_x[0] = info.pos.x;
			var->point_y[0] = info.pos.y;
		}
		if (!filter->use_start_scale) {
			var->scale_x[0] = info.scale.x;
			var->scale_y[0] = info.scale.y;
		}

	}

	if (filter->use_start_position){
		var->point_x[0] = filter->org_pos.x;
		var->point_y[0] = filter->org_pos.y;
	}

	if (filter->path_type >= PATH_QUADRATIC) {
		var->point_x[1] = filter->ctrl_pos.x;
		var->point_y[1] = filter->ctrl_pos.y;
	}
		
	if (filter->path_type == PATH_CUBIC) {
		var->point_x[2] = filter->ctrl2_pos.x;
		var->point_y[2] = filter->ctrl2_pos.y;
	}
		
	var->point_x[filter->path_type + 1] = filter->dst_pos.x;
	var->point_y[filter->path_type + 1] = filter->dst_pos.y;

	if(filter->use_start_scale) {
		cal_scale(filter->item, &var->scale_x[0],
			&var->scale_y[0], filter->org_width, filter->org_height);
	}

	cal_scale(filter->item, &var->scale_x[1],
		&var->scale_y[1], filter->dst_width, filter->dst_height);

	if (filter->acceleration != 0) {
		var->coeff_varaite = true;
		var->coeff[0] = 0.0f;
		var->coeff[1] = (-(filter->acceleration) + 1.0f) / 2;
		var->coeff[2] = 1.0f;
	} else
		var->coeff_varaite = false;

	var->elapsed_time = 0.0f;
	return ;
}

static void reset_source_name(void *data, obs_sceneitem_t *item)
{
	motion_filter_data_t *filter = data;
	if (item) {
		obs_data_t *settings = obs_source_get_settings(filter->context);
		obs_source_t *item_source = obs_sceneitem_get_source(item);
		const char *name = obs_source_get_name(item_source);
		obs_data_set_string(settings, S_SOURCE, name);
		bfree(filter->item_name);
		filter->item_name = bstrdup(name);
		obs_data_release(settings);
	}
}

static void recover_source(motion_filter_data_t *filter)
{
	struct vec2 pos;
	struct vec2 scale;
	obs_data_t *settings;
	variation_data_t *var = &filter->variation;

	if (!filter->motion_end)
		return;

	pos.x = var->point_x[0];
	pos.y = var->point_y[0];
	scale.x = var->scale_x[0];
	scale.y = var->scale_y[0];

	obs_sceneitem_set_pos(filter->item, &pos);
	obs_sceneitem_set_scale(filter->item, &scale);
	filter->motion_end = false;
	settings = obs_source_get_settings(filter->context);
	obs_data_set_bool(settings, S_MOTION_END, false);
	obs_data_release(settings);
}

static bool motion_init(void *data, bool forward)
{
	motion_filter_data_t *filter = data;

	if (filter->motion_start || is_reverse(filter) == forward)
		return false;

	filter->item = get_item(filter->context, filter->item_name);

	if (!filter->item) {
		filter->item = get_item_by_id(filter->context, filter->item_id);
		reset_source_name(data, filter->item);
	}

	if (filter->item) {
		update_variation_data(filter);
		obs_sceneitem_addref(filter->item);
		filter->motion_start = true;
		return true;
	}
	return false;
}

static bool hotkey_forward(void *data, obs_hotkey_pair_id id,
	obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(pressed);
	return motion_init(data, true);
}

static bool hotkey_backward(void *data, obs_hotkey_pair_id id,
	obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);
	UNUSED_PARAMETER(pressed);
	return motion_init(data, false);
}

static void scene_change(enum obs_frontend_event event, void *data)
{
	motion_filter_data_t *filter = data;
	obs_data_t *settings;
	obs_source_t *cur_scene;
	obs_source_t *self_scene;
	const char* cur_name;
	const char* self_name;

	if (event != OBS_FRONTEND_EVENT_SCENE_CHANGED)
		return;

	cur_scene = obs_frontend_get_current_scene();
	self_scene = obs_filter_get_parent(filter->context);

	if (cur_scene == self_scene) {
		motion_init(data, true);
	} else if (is_program_scene(self_scene)) {
		settings = obs_source_get_settings(filter->context);
		self_name = obs_data_get_string(settings, S_SCENE_NAME);
		cur_name = obs_source_get_name(cur_scene);
		if (self_name && cur_name && strcmp(self_name, cur_name)==0) {
			motion_init(data, true);
		}
		obs_data_release(settings);
	} else {
		filter->motion_start = false;
		filter->motion_end = true;
		recover_source(filter);
	}
	obs_source_release(cur_scene);
}

static void set_reverse_info(struct motion_filter_data *filter)
{
	variation_data_t *var = &filter->variation;
	obs_data_t *settings = obs_source_get_settings(filter->context);
	obs_data_set_bool(settings, S_MOTION_END, filter->motion_end);
	obs_data_set_double(settings, S_ORG_X, var->point_x[0]);
	obs_data_set_double(settings, S_ORG_Y, var->point_y[0]);
	obs_data_set_double(settings, S_ORG_W, var->scale_x[0]);
	obs_data_set_double(settings, S_ORG_H, var->scale_y[0]);
	obs_data_release(settings);
}

static void get_reverse_info(struct motion_filter_data *filter)
{
	variation_data_t *var = &filter->variation;
	obs_data_t *settings = obs_source_get_settings(filter->context);
	filter->motion_end = obs_data_get_bool(settings, S_MOTION_END);
	var->point_x[0] = (float)obs_data_get_double(settings, S_ORG_X);
	var->point_y[0] = (float)obs_data_get_double(settings, S_ORG_X);
	var->scale_x[0] = (float)obs_data_get_double(settings, S_ORG_W);
	var->scale_y[0] = (float)obs_data_get_double(settings, S_ORG_H);
	obs_data_release(settings);
}

static void motion_filter_save(void *data, obs_data_t *settings)
{
	motion_filter_data_t *filter = data;
	const char *name = get_scene_name(filter);

	if (name) 
		obs_data_set_string(settings, S_SCENE_NAME, name);

	save_hotkey_config(filter->hotkey_id_f, settings, S_FORWARD);
	save_hotkey_config(filter->hotkey_id_b, settings, S_BACKWARD);
}

static void motion_filter_update(void *data, obs_data_t *settings)
{
	motion_filter_data_t *filter = data;
	bool use_start, change_pos, change_size, scene_switch;
	int var_type;
	int64_t item_id;
	const char *item_name;

	filter->motion_behavior = (int)obs_data_get_int(settings, S_MOTION_BEHAVIOR);
	filter->path_type = (int)obs_data_get_int(settings, S_PATH_TYPE);
	filter->org_pos.x = (float)obs_data_get_int(settings, S_START_X);
	filter->org_pos.y = (float)obs_data_get_int(settings, S_START_Y);
	filter->org_width = (int)obs_data_get_int(settings, S_START_W);
	filter->org_height = (int)obs_data_get_int(settings, S_START_H);
	filter->ctrl_pos.x = (float)obs_data_get_int(settings, S_CTRL_X);
	filter->ctrl_pos.y = (float)obs_data_get_int(settings, S_CTRL_Y);
	filter->ctrl2_pos.x = (float)obs_data_get_int(settings, S_CTRL2_X);
	filter->ctrl2_pos.y = (float)obs_data_get_int(settings, S_CTRL2_Y);
	filter->duration = (float)obs_data_get_double(settings, S_DURATION);
	filter->dst_pos.x = (float)obs_data_get_int(settings, S_DST_X);
	filter->dst_pos.y = (float)obs_data_get_int(settings, S_DST_Y);
	filter->dst_width = (int)obs_data_get_int(settings, S_DST_W);
	filter->dst_height = (int)obs_data_get_int(settings, S_DST_H);
	filter->acceleration = (float)obs_data_get_double(settings, S_ACCELERATION);
	use_start = obs_data_get_bool(settings, S_START_SETTING);
	var_type = (int)obs_data_get_int(settings, S_VARIATION_TYPE);
	item_name = obs_data_get_string(settings, S_SOURCE);
	item_id = get_item_id(filter->context, item_name);

	change_pos = (var_type & VARIATION_POSITION) != 0;
	change_size = (var_type & VARIATION_SIZE) != 0;
	scene_switch = filter->motion_behavior == BEHAVIOR_SCENE_SWITCH;


	filter->use_start_position = (scene_switch || use_start) && change_pos;
	filter->use_start_scale = (scene_switch || use_start) && change_size;
	filter->change_position = change_pos;
	filter->change_size = change_size;


	bfree(filter->item_name);
	filter->item_name = bstrdup(item_name);
	filter->item_id = item_id;
}

static bool register_trigger_event(void *data)
{
	motion_filter_data_t *filter = data;
	obs_source_t *source = obs_filter_get_parent(filter->context);
	obs_scene_t *scene = obs_scene_from_source(source);

	if (!scene)
		return false;

	if (filter->motion_behavior == BEHAVIOR_SCENE_SWITCH) {
		obs_frontend_add_event_callback(scene_change, data);
		return true;
	}


	filter->hotkey_id_f = register_hotkey(filter->context, source, S_FORWARD,
		T_FORWARD, hotkey_forward, data);

	if (filter->motion_behavior == BEHAVIOR_ROUND_TRIP) {
		filter->hotkey_id_b = register_hotkey(filter->context, source, 
			S_BACKWARD, T_BACKWARD, hotkey_backward, data);
	}

	return true;
}

static void unregister_trigger_event(void *data)
{
	motion_filter_data_t *filter = data;
	obs_data_t *settings;


	if (filter->motion_behavior == BEHAVIOR_SCENE_SWITCH) {
		obs_frontend_remove_event_callback(scene_change, data);
		return ;
	}

	settings = obs_source_get_settings(filter->context);
	motion_filter_save(data, settings);
	obs_data_release(settings);


	unregister_hotkey(filter->hotkey_id_f);
	unregister_hotkey(filter->hotkey_id_b);
	filter->hotkey_id_f = OBS_INVALID_HOTKEY_ID;
	filter->hotkey_id_b = OBS_INVALID_HOTKEY_ID;
}

static bool motion_set_button(obs_properties_t *props, obs_property_t *p,
	bool reversed)
{
	obs_property_t *f = obs_properties_get(props, S_FORWARD);
	obs_property_t *b = obs_properties_get(props, S_BACKWARD);
	obs_property_set_visible(f, !reversed);
	obs_property_set_visible(b, reversed);
	UNUSED_PARAMETER(p);
	return true;
}

static bool forward_clicked(obs_properties_t *props, obs_property_t *p,
	void *data)
{
	motion_filter_data_t *filter = data;
	if (motion_init(data, true) && filter->motion_behavior == BEHAVIOR_ROUND_TRIP)
		return motion_set_button(props, p, true);
	else
		return false;
}

static bool backward_clicked(obs_properties_t *props, obs_property_t *p,
	void *data)
{
	if (motion_init(data, false))
		return motion_set_button(props, p, false);
	else
		return false;
}

static bool source_changed(void *data, obs_properties_t *props, 
	obs_property_t *p, obs_data_t *s)
{
	motion_filter_data_t* filter = data;
	const char* name = obs_data_get_string(s, S_SOURCE);
	
	if (!filter->item_name ||!name)
		return false;
	else if (strcmp(filter->item_name, name) == 0)
		return false;
	else 
		recover_source(filter);

	return motion_set_button(props, p, false);
}

static bool motion_list_source(obs_scene_t* scene,
	obs_sceneitem_t* item, void* p)
{
	obs_source_t *source = obs_sceneitem_get_source(item);
	const char *name = obs_source_get_name(source);
	obs_property_list_add_string((obs_property_t*)p, name, name);
	UNUSED_PARAMETER(scene);
	return true;
}

/** 
 * Macro: set_visibility
 * ---------------------
 * Sets the visibility of a property field in the config.
 *		key:	the property key - e.g. S_EXAMPLE
 *		vis:	a bool for whether the property should be shown (true) or hidden (false)
 */
#define set_visibility(key, vis) \
		do { \
			p = obs_properties_get(props, key); \
			obs_property_set_visible(p, vis);\
		} while (false)


static bool properties_set_vis(void *data, obs_properties_t *props,
	obs_property_t *p, obs_data_t *s)
{
	int trigger_type = (int)obs_data_get_int(s, S_MOTION_BEHAVIOR);
	int var_type = (int)obs_data_get_int(s, S_VARIATION_TYPE);
	int path_type = (int)obs_data_get_int(s, S_PATH_TYPE);
	bool use_start = obs_data_get_bool(s, S_START_SETTING);
	bool change_pos = (var_type & VARIATION_POSITION) != 0;
	bool change_size = (var_type & VARIATION_SIZE) != 0;
	bool scene_switch = trigger_type == BEHAVIOR_SCENE_SWITCH;

	set_visibility(S_START_SETTING, !scene_switch);
	set_visibility(S_START_X, change_pos && (use_start || scene_switch));
	set_visibility(S_START_Y, change_pos && (use_start || scene_switch));
	set_visibility(S_DST_X, change_pos);
	set_visibility(S_DST_Y, change_pos);
	set_visibility(S_PATH_TYPE, change_pos);
	set_visibility(S_CTRL_X, change_pos && path_type >= PATH_QUADRATIC);
	set_visibility(S_CTRL_Y, change_pos && path_type >= PATH_QUADRATIC);
	set_visibility(S_CTRL2_X, change_pos && path_type >= PATH_CUBIC);
	set_visibility(S_CTRL2_Y, change_pos && path_type >= PATH_CUBIC);
	set_visibility(S_START_W, change_size && (use_start || scene_switch));
	set_visibility(S_START_H, change_size && (use_start || scene_switch));
	set_visibility(S_DST_W, change_size);
	set_visibility(S_DST_H, change_size);

	UNUSED_PARAMETER(p);
	return true;
}

static bool motion_behavior_changed(void *data, obs_properties_t *props,
	obs_property_t *p, obs_data_t *s)
{
	motion_filter_data_t *filter = data;
	int behavior = (int)obs_data_get_int(s, S_MOTION_BEHAVIOR);
	if (behavior != filter->motion_behavior) {
		recover_source(filter);
		unregister_trigger_event(data);
		filter->motion_behavior = behavior;
		register_trigger_event(data);
		properties_set_vis(data, props, p, s);
		return motion_set_button(props, p, false);
	}
	return false;
}

static bool dest_grab_current_position_clicked(obs_properties_t *props, 
	obs_property_t *p, void *data)
{
	struct motion_filter_data *filter = data;
	obs_sceneitem_t *item = get_item(filter->context, filter->item_name);
	// Find the targetted source item within the scene
	if (!filter->item) {
		item = get_item_by_id(filter->context, filter->item_id);
		reset_source_name(data, filter->item);
	}

	if (item) {
		struct obs_transform_info info;
		int width, height;
		obs_sceneitem_get_info(item, &info);
		cal_size(item, info.scale.x, info.scale.y, &width, &height);
		// Set setting property values to match the source's current position
		obs_data_t *settings = obs_source_get_settings(filter->context);
		obs_data_set_int(settings, S_DST_X, (int)info.pos.x);
		obs_data_set_int(settings, S_DST_Y, (int)info.pos.y);
		obs_data_set_int(settings, S_DST_W, width);
		obs_data_set_int(settings, S_DST_H, height);
		obs_data_release(settings);
		return true;
	}

	return false;
}

#undef set_visibility
#undef set_visibility_bool

/*
 * Filter property layout.
 */
static obs_properties_t *motion_filter_properties(void *data)
{
	motion_filter_data_t *filter = data;
	obs_properties_t *props = obs_properties_create();
	obs_property_t *p;
	struct dstr disable_str = { 0 };

	obs_source_t *source = obs_filter_get_parent(filter->context);
	obs_scene_t *scene = obs_scene_from_source(source);

	if (!scene)
		return props;
	
	p = obs_properties_add_list(props, S_SOURCE, T_SOURCE,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	dstr_copy(&disable_str, "--- ");
	dstr_cat(&disable_str, T_DISABLED);
	dstr_cat(&disable_str, " ---");
	obs_property_list_add_string(p, disable_str.array, disable_str.array);
	dstr_free(&disable_str);

	// A list of sources
	obs_scene_enum_items(scene, motion_list_source, (void*)p);
	obs_property_set_modified_callback2(p, source_changed, filter);

	// Various motion behaviour types
	p = obs_properties_add_list(props, S_MOTION_BEHAVIOR, T_MOTION_BEHAVIOR,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, T_HOTKEY_ONE_WAY, BEHAVIOR_ONE_WAY);
	obs_property_list_add_int(p, T_HOTKEY_ROUND_TRIP, BEHAVIOR_ROUND_TRIP);
	obs_property_list_add_int(p, T_SCENE_SWITCH, BEHAVIOR_SCENE_SWITCH);
	// Using modified_callback2 enables us to send along data into the callback
	obs_property_set_modified_callback2(p, motion_behavior_changed, filter);


	//Variation of position or size
	p = obs_properties_add_list(props, S_VARIATION_TYPE, T_VARIATION_TYPE,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, T_VARIATION_POS, VARIATION_POSITION);
	obs_property_list_add_int(p, T_VARIATION_SIZE, VARIATION_SIZE);
	obs_property_list_add_int(p, T_VARIATION_BOTH, 
		VARIATION_POSITION | VARIATION_SIZE);
	obs_property_set_modified_callback2(p, properties_set_vis, filter);

	p = obs_properties_add_bool(props, S_START_SETTING, T_START_SETTING);
	obs_property_set_modified_callback2(p, properties_set_vis,filter);

	// Custom starting X and Y values
	obs_properties_add_int(props, S_START_X, T_START_X, -8192, 8192, 1);
	obs_properties_add_int(props, S_START_Y, T_START_Y, -8192, 8192, 1);


	// Custom width and height
	obs_properties_add_int(props, S_START_W, T_START_W, 0, 8192, 1);
	obs_properties_add_int(props, S_START_H, T_START_H, 0, 8192, 1);

	// Various animation types
	p = obs_properties_add_list(props, S_PATH_TYPE, T_PATH_TYPE,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, T_PATH_LINEAR, PATH_LINEAR);
	obs_property_list_add_int(p, T_PATH_QUADRATIC, PATH_QUADRATIC);
	obs_property_list_add_int(p, T_PATH_CUBIC, PATH_CUBIC);
	obs_property_set_modified_callback2(p, properties_set_vis,filter);

	// Button that pre-populates destination position with the source's current position
	obs_properties_add_button(props, S_DEST_GRAB_POS, T_DEST_GRAB_POS,
		dest_grab_current_position_clicked);

	// Destination X and Y values
	obs_properties_add_int(props, S_DST_X, T_DST_X, -8192, 8192, 1);
	obs_properties_add_int(props, S_DST_Y, T_DST_Y, -8192, 8192, 1);


	// Other control point fields for other types
	obs_properties_add_int(props, S_CTRL_X, T_CTRL_X, -8192, 8192, 1);
	obs_properties_add_int(props, S_CTRL_Y, T_CTRL_Y, -8192, 8192, 1);
	obs_properties_add_int(props, S_CTRL2_X, T_CTRL2_X, -8192, 8192, 1);
	obs_properties_add_int(props, S_CTRL2_Y, T_CTRL2_Y, -8192, 8192, 1);

	// Custom width and height
	obs_properties_add_int(props, S_DST_W, T_DST_W, 0, 8192, 1);
	obs_properties_add_int(props, S_DST_H, T_DST_H, 0, 8192, 1);

	// Animation duration slider
	obs_properties_add_float_slider(props, S_DURATION, T_DURATION, 0, 5, 
		0.1);

	// Animation acceleration slider
	obs_properties_add_float_slider(props, S_ACCELERATION, T_ACCELERATION, -1, 
		1, 0.01);

	// Forwards / Backwards button(s)
	p = obs_properties_add_button(props, S_FORWARD, T_FORWARD, forward_clicked);
	obs_property_set_visible(p, !is_reverse(filter));
	p =obs_properties_add_button(props, S_BACKWARD, T_BACKWARD, backward_clicked);
	obs_property_set_visible(p, is_reverse(filter));

	return props;
}

static void cal_variation(motion_filter_data_t *filter)
{
	variation_data_t *var = &filter->variation;

	float elapsed_time = min(filter->duration, var->elapsed_time);
	float coeff;
	int order;

	if (filter->duration <= 0)
		coeff = 1.0f;
	else if (is_reverse(filter)) 
		coeff = 1.0f - (elapsed_time / filter->duration);
	else 
		coeff = elapsed_time / filter->duration;

	if (var->coeff_varaite)
		coeff = bezier(var->coeff, coeff, 2);

	order = filter->change_size ? 1 : 0;
	
	var->scale.x = bezier(var->scale_x, coeff, order);
	var->scale.y = bezier(var->scale_y, coeff, order);


	if (!filter->change_position)
		order = 0;
	else if (filter->path_type == PATH_QUADRATIC)
		order = 2;
	else if (filter->path_type == PATH_CUBIC)
		order = 3;
	else 
		order = 1;

	var->position.x = bezier(var->point_x, coeff, order);
	var->position.y = bezier(var->point_y, coeff, order);
}

static void motion_filter_tick(void *data, float seconds)
{
	motion_filter_data_t *filter = data;
	variation_data_t *var = &filter->variation;

	if (filter->motion_start) {

		cal_variation(filter);
		obs_sceneitem_set_pos(filter->item, &var->position);
		obs_sceneitem_set_scale(filter->item, &var->scale);

		if (var->elapsed_time >= filter->duration) {
			filter->motion_start = false;
			var->elapsed_time = 0.0f;
			obs_sceneitem_release(filter->item);
			filter->motion_end = !filter->motion_end;
			set_reverse_info(filter);
		} else
			var->elapsed_time += seconds;
	}


	//Some APIs are not valid during creation , do initlize in tick loop
	if (!filter->initialize) {
		register_trigger_event(data);
		obs_data_t* settings = obs_source_get_settings(filter->context);
		motion_filter_save(data, settings);
		obs_data_release(settings);
		filter->initialize = true;
	}
}

static void *motion_filter_create(obs_data_t *settings, obs_source_t *context)
{
	motion_filter_data_t *filter = bzalloc(sizeof(*filter));
	
	filter->context = context;
	filter->motion_start = false;
	filter->initialize = false;
	filter->motion_behavior = BEHAVIOR_ROUND_TRIP;
	filter->path_type = PATH_LINEAR;
	filter->hotkey_id_f = OBS_INVALID_HOTKEY_ID;
	filter->hotkey_id_b = OBS_INVALID_HOTKEY_ID;
	get_reverse_info(filter);
	obs_source_update(context, settings);
	return filter;
}

static void motion_filter_remove(void *data, obs_source_t *source)
{
	motion_filter_data_t *filter = data;
	unregister_trigger_event(data);
	recover_source(filter);
	UNUSED_PARAMETER(source);
}

static void motion_filter_destroy(void *data)
{
	motion_filter_data_t *filter = data;
	bfree(filter->item_name);
	bfree(filter);
}

static void motion_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, S_MOTION_END, false);
	obs_data_set_default_int(settings, S_MOTION_BEHAVIOR, BEHAVIOR_ROUND_TRIP);
	obs_data_set_default_double(settings, S_DURATION, 1.0);
}

static const char *motion_filter_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return T_("Motion");
}

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("motion-transitions", "en-US")

struct obs_source_info motion_filter = {
	.id = "motion-filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = motion_filter_get_name,
	.create = motion_filter_create,
	.destroy = motion_filter_destroy,
	.update = motion_filter_update,
	.get_properties = motion_filter_properties,
	.get_defaults = motion_filter_defaults,
	.video_tick = motion_filter_tick,
	.save = motion_filter_save,
	.filter_remove = motion_filter_remove
};

bool obs_module_load(void) {
	obs_register_source(&motion_filter);
	return true;
}

