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
#include <util/dstr.h>

// Define property keys
#define	S_PATH_LINEAR       0
#define S_PATH_QUADRATIC    1
#define S_PATH_CUBIC        2
#define S_IS_REVERSED       "is_reversed"
#define S_ORG_X             "org_x"
#define S_ORG_Y             "org_y"
#define S_ORG_W             "org_w"
#define S_ORG_H             "org_h"
#define S_PATH_TYPE         "path_type"
#define S_START_POS         "start_position"
#define S_START_SCALE       "start_scale"
#define S_CTRL_X            "ctrl_x"
#define S_CTRL_Y            "ctrl_y"
#define S_CTRL2_X           "ctrl2_x"
#define S_CTRL2_Y           "ctrl2_y"
#define S_DST_X             "dst_x"
#define S_DST_Y             "dst_y"
#define S_DST_W             "dst_w"
#define S_DST_H             "dst_h"
#define S_USE_DST_SCALE     "dst_use_scale"
#define	S_DURATION          "duration"
#define S_SOURCE            "source_id"
#define S_FORWARD           "forward"
#define S_BACKWARD          "backward"
#define S_DEST_GRAB_POS     "use_cur_src_pos"

// Define property localisation tags
#define T_(v)               obs_module_text(v)
#define T_PATH_TYPE         T_("PathType")
#define T_START_POS         T_("Start.GivenPosition")
#define T_START_SCALE       T_("Start.GivenScale")
#define	T_PATH_LINEAR       T_("PathType.Linear")
#define T_PATH_QUADRATIC    T_("PathType.Quadratic")
#define T_PATH_CUBIC        T_("PathType.Cubic")
#define T_ORG_X             T_("Start.X")
#define T_ORG_Y             T_("Start.Y")
#define T_ORG_W             T_("Start.W")
#define T_ORG_H             T_("Start.H")
#define T_CTRL_X            T_("ControlPoint.X")
#define T_CTRL_Y            T_("ControlPoint.Y")
#define T_CTRL2_X           T_("ControlPoint2.X")
#define T_CTRL2_Y           T_("ControlPoint2.Y")
#define T_DST_X             T_("Destination.X")
#define T_DST_Y             T_("Destination.Y")
#define T_DST_W             T_("Destination.W")
#define T_DST_H             T_("Destination.H")
#define T_USE_DST_SCALE     T_("ChangeScale")
#define T_DURATION          T_("Duration")
#define T_SOURCE            T_("SourceName")
#define T_FORWARD           T_("Forward")
#define T_BACKWARD          T_("Backward")
#define T_DISABLED          T_("Disabled")
#define T_DEST_GRAB_POS     T_("DestinationGrabPosition")

struct motion_filter_data {
	obs_source_t        *context;
	obs_scene_t         *scene;
	obs_sceneitem_t     *item;
	obs_hotkey_id		hotkey_id_f;
	obs_hotkey_id       hotkey_id_b;
	bool                round_trip;
	bool                hotkey_init;
	bool                restart_backward;
	bool                motion_start;
	bool                motion_reverse;
	bool                start_position;
	bool                start_scale;
	bool                use_dst_scale;
	int                 path_type;
	int                 org_width;
	int                 org_height;
	int                 dst_width;
	int                 dst_height;
	struct vec2         org_pos;
	struct vec2         ctrl_pos;
	struct vec2         ctrl2_pos;
	struct vec2         dst_pos;
	struct vec2         org_scale;
	struct vec2         dst_scale;
	struct vec2         position;
	struct vec2         scale;
	float               duration;
	float               elapsed_time;
	char                *item_name;
	int64_t             item_id;

};

static inline obs_sceneitem_t *get_item(obs_source_t* context, 
	const char* name)
{
	obs_source_t *source = obs_filter_get_parent(context);
	obs_scene_t *scene = obs_scene_from_source(source);
	return obs_scene_find_source(scene, name);
}

static inline obs_sceneitem_t* get_item_by_id(void *data, int64_t id)
{
	struct motion_filter_data *filter = data;
	obs_source_t* source = obs_filter_get_parent(filter->context);
	obs_scene_t* scene = obs_scene_from_source(source);
	obs_sceneitem_t* item = obs_scene_find_sceneitem_by_id(scene, id);
	if (item){
		obs_data_t *settings = obs_source_get_settings(filter->context);
		obs_source_t *item_source = obs_sceneitem_get_source(item);
		const char *name = obs_source_get_name(item_source);
		obs_data_set_string(settings, S_SOURCE, name);
		bfree(filter->item_name);
		filter->item_name = bstrdup(name);
		obs_data_release(settings);
	}
	return item;
}

static inline int64_t get_item_id(obs_source_t* context, const char* name)
{
	obs_sceneitem_t* item = get_item(context, name);
	return item ? obs_sceneitem_get_id(item) : -1;
}

static bool cal_size(obs_sceneitem_t* item, float sx, float sy,
	int* width, int* height)
{
	obs_source_t* item_source = obs_sceneitem_get_source(item);
	int base_width = obs_source_get_base_width(item_source);
	int base_height = obs_source_get_base_height(item_source);

	*width = (int)(base_width * sx);
	*height = (int)(base_width * sy);

	return true;
}

static bool cal_scale(obs_sceneitem_t* item, float* sx, float*sy,
	int width, int height)
{
	obs_source_t* item_source = obs_sceneitem_get_source(item);
	int base_width = obs_source_get_width(item_source);
	int base_height = obs_source_get_height(item_source);

	if (base_width == 0 || base_height == 0)
		return false;

	*sx = (float)width / base_width;
	*sy = (float)height / base_height;

	return true;
}

static inline void set_item_scale(obs_sceneitem_t* item, int width, int height)
{
	struct vec2 scale;
	if (cal_scale(item, &scale.x, &scale.y, width, height))
		obs_sceneitem_set_scale(item, &scale);
}

static const char *motion_filter_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return T_("Motion");
}

static const char *motion_filter_round_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return T_("RoundTripMotion");
}

static bool motion_init(void *data, bool forward)
{
	struct motion_filter_data *filter = data;

	if (filter->motion_start || filter->motion_reverse == forward)
		return false;

	filter->item = get_item(filter->context, filter->item_name);

	if (!filter->item)
		filter->item = get_item_by_id(filter, filter->item_id);

	if (filter->item){

		if (!cal_scale(filter->item, &filter->dst_scale.x,
			&filter->dst_scale.y, filter->dst_width, filter->dst_height))
			return false;

		if (!filter->motion_reverse){
			struct obs_transform_info info;
			obs_sceneitem_get_info(filter->item, &info);

			if (!filter->start_position){
				filter->org_pos.x = info.pos.x;
				filter->org_pos.y = info.pos.y;
			}

			if (!filter->start_scale){
				filter->org_scale.x = info.scale.x;
				filter->org_scale.y = info.scale.y;
			}
			else
				cal_scale(filter->item, &filter->org_scale.x,
				&filter->org_scale.y, filter->org_width, filter->org_width);
		}

		obs_sceneitem_addref(filter->item);
		filter->elapsed_time = 0.0f;
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

static void set_reverse_info(struct motion_filter_data *filter)
{
	obs_data_t *settings = obs_source_get_settings(filter->context);
	obs_data_set_bool(settings, S_IS_REVERSED, filter->motion_reverse);
	obs_data_set_int(settings, S_ORG_X, (int)filter->org_pos.x);
	obs_data_set_int(settings, S_ORG_Y, (int)filter->org_pos.y);
	obs_data_set_int(settings, S_ORG_W, filter->org_width);
	obs_data_set_int(settings, S_ORG_H, filter->org_height);
	obs_data_release(settings);
}

static void motion_filter_update(void *data, obs_data_t *settings)
{
	struct motion_filter_data *filter = data;
	struct vec2	pos;
	int width;
	int height;
	int64_t item_id;
	const char *item_name;

	filter->start_position = obs_data_get_bool(settings, S_START_POS);
	filter->start_scale = obs_data_get_bool(settings, S_START_SCALE);
	filter->path_type = (int)obs_data_get_int(settings, S_PATH_TYPE);
	filter->org_pos.x = (float)obs_data_get_int(settings, S_ORG_X);
	filter->org_pos.y = (float)obs_data_get_int(settings, S_ORG_Y);
	filter->org_width = (int)obs_data_get_int(settings, S_ORG_W);
	filter->org_height = (int)obs_data_get_int(settings, S_ORG_H);
	filter->ctrl_pos.x = (float)obs_data_get_int(settings, S_CTRL_X);
	filter->ctrl_pos.y = (float)obs_data_get_int(settings, S_CTRL_Y);
	filter->ctrl2_pos.x = (float)obs_data_get_int(settings, S_CTRL2_X);
	filter->ctrl2_pos.y = (float)obs_data_get_int(settings, S_CTRL2_Y);
	filter->duration = (float)obs_data_get_double(settings, S_DURATION);
	filter->use_dst_scale = obs_data_get_bool(settings, S_USE_DST_SCALE);
	item_name = obs_data_get_string(settings, S_SOURCE);
	item_id = get_item_id(filter->context, item_name);
	pos.x = (float)obs_data_get_int(settings, S_DST_X);
	pos.y = (float)obs_data_get_int(settings, S_DST_Y);
	width = (int)obs_data_get_int(settings, S_DST_W);
	height = (int)obs_data_get_int(settings, S_DST_H);


	if (filter->motion_reverse){
		obs_sceneitem_t *item = get_item(filter->context, item_name);

		if (filter->restart_backward){
			obs_sceneitem_set_pos(item, &filter->org_pos);
			set_item_scale(item, filter->org_width, filter->org_height);
			filter->motion_reverse = false;
			filter->restart_backward = false;
		}
		else if (item_id != filter->item_id){
			obs_sceneitem_set_pos(filter->item, &filter->org_pos);
			obs_sceneitem_set_scale(filter->item, &filter->org_scale);
			filter->motion_reverse = false;
		}
		else if (pos.x != filter->dst_pos.x || pos.y != filter->dst_pos.y)
			obs_sceneitem_set_pos(item, &pos);
		else if (width != filter->dst_width || height != filter->dst_height)
			set_item_scale(item, width, height);
	}

	filter->dst_pos.x = pos.x;
	filter->dst_pos.y = pos.y;
	filter->dst_width = width;
	filter->dst_height = height;
	bfree(filter->item_name);
	filter->item_name = bstrdup(item_name);
	filter->item_id = item_id;
}

static void register_hothey(struct motion_filter_data *filter, const char *type,
	const char *text, obs_source_t *source, obs_hotkey_func func)
{
	const char *name = obs_source_get_name(filter->context);
	const char *s_name = obs_source_get_name(source);
	obs_data_t *settings = obs_source_get_settings(filter->context);
	obs_data_array_t *save_array;
	struct dstr str = { 0 };
	dstr_copy(&str, text);
	dstr_cat(&str, " [ %1 ] ");
	dstr_replace(&str, "%1", name);
	const char *description = str.array;
	obs_hotkey_id id;

	if (s_name)
		id = obs_hotkey_register_source(source,description, description, 
		func, filter);
	else
		id = obs_hotkey_register_frontend(description, description, func, 
		filter);

	if (strcmp(type, S_FORWARD) == 0)
		filter->hotkey_id_f = id;
	else
		filter->hotkey_id_b = id;

	save_array = obs_data_get_array(settings, type);
	obs_hotkey_load(id, save_array);
	obs_data_array_release(save_array);
	dstr_free(&str);
	obs_data_release(settings);
}

static bool init_hotkey(void *data)
{
	struct motion_filter_data *filter = data;
	obs_source_t *source = obs_filter_get_parent(filter->context);
	obs_scene_t *scene = obs_scene_from_source(source);
	filter->hotkey_init = true;

	if (!scene)
		return false;

	register_hothey(filter, S_FORWARD, T_FORWARD, source, hotkey_forward);

	if (filter->round_trip)
		register_hothey(filter, S_BACKWARD, T_BACKWARD, source, hotkey_backward);
		
	return true;
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
	struct motion_filter_data *filter = data;
	if (motion_init(data, true) && filter->round_trip)
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

static bool source_changed(obs_properties_t *props, obs_property_t *p,
	obs_data_t *s)
{
	bool reversed = obs_data_get_bool(s, S_IS_REVERSED);
	obs_property_t *f = obs_properties_get(props, S_FORWARD);
	obs_property_t *b = obs_properties_get(props, S_BACKWARD);
	if (obs_property_visible(f) && obs_property_visible(b))
		return motion_set_button(props, p, reversed);
	else
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
 * Our lists have an int backend like an enum,
 *		key:	the property key - e.g. S_EXAMPLE
 *		val:	either 0 or 1 for toggles, 0->N for lists
 *		cmp:	comparison value, either 1 for toggles, or 0->N for lists
 */
#define set_visibility(key, val, cmp) \
		do { \
			p = obs_properties_get(props, key); \
			obs_property_set_visible(p, val >= cmp);\
		} while (false)

/* 
 * Macro: set_visibility_bool
 * --------------------------
 * Shorthand for when we want visibility directly affected by toggle. 
 *		key:	the property key - e.g. S_EXAMPLE
 *		vis:	a bool for whether the property should be shown (true) or hidden (false)
 */
#define set_visibility_bool(key, vis) \
		set_visibility(key, vis ? 1 : 0, 1)

static bool path_type_changed(obs_properties_t *props, obs_property_t *p,
	obs_data_t *s)
{
	int type = (int)obs_data_get_int(s, S_PATH_TYPE);
	set_visibility(S_CTRL_X, type, S_PATH_QUADRATIC);
	set_visibility(S_CTRL_Y, type, S_PATH_QUADRATIC);
	set_visibility(S_CTRL2_X, type, S_PATH_CUBIC);
	set_visibility(S_CTRL2_Y, type, S_PATH_CUBIC);
	return true;
}

static bool provide_start_position_toggle_changed(obs_properties_t *props, 
	obs_property_t *p, obs_data_t *s)
{
	bool ticked = obs_data_get_bool(s, S_START_POS);
	set_visibility_bool(S_ORG_X, ticked);
	set_visibility_bool(S_ORG_Y, ticked);
	return true;
}

static bool provide_start_size_toggle_changed(obs_properties_t *props, 
	obs_property_t *p, obs_data_t *s)
{
	bool ticked = obs_data_get_bool(s, S_START_SCALE);
	set_visibility_bool(S_ORG_W, ticked);
	set_visibility_bool(S_ORG_H, ticked);
	return true;
}

static bool provide_custom_size_at_destination_toggle_changed(
	obs_properties_t *props, obs_property_t *p, obs_data_t *s)
{
	bool ticked = obs_data_get_bool(s, S_USE_DST_SCALE);
	set_visibility_bool(S_DST_W, ticked);
	set_visibility_bool(S_DST_H, ticked);
	return true;
}

static bool dest_grab_current_position_clicked(obs_properties_t *props, 
	obs_property_t *p, void *data)
{
	struct motion_filter_data *filter = data;
	obs_sceneitem_t *item = get_item(filter->context, filter->item_name);
	// Find the targetted source item within the scene
	if (!filter->item)
		item = get_item_by_id(filter, filter->item_id);

	if (item) {
		struct obs_transform_info info;
		obs_sceneitem_get_info(item, &info);
		// Set setting property values to match the source's current position
		obs_data_t *settings = obs_source_get_settings(filter->context);
		obs_data_set_double(settings, S_DST_X, info.pos.x);
		obs_data_set_double(settings, S_DST_Y, info.pos.y);
		obs_data_release(settings);
	}

	return true;
}

#undef set_visibility
#undef set_visibility_bool

/*
 * Filter property layout.
 */
static obs_properties_t *motion_filter_properties(void *data)
{
	struct motion_filter_data *filter = data;
	obs_properties_t *props = obs_properties_create();
	obs_property_t *p;

	obs_source_t *source = obs_filter_get_parent(filter->context);
	obs_scene_t *scene = obs_scene_from_source(source);

	if (!scene)
		return props;
	
	p = obs_properties_add_list(props, S_SOURCE, T_SOURCE,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	struct dstr disable_str = { 0 };
	dstr_copy(&disable_str, "--- ");
	dstr_cat(&disable_str, T_DISABLED);
	dstr_cat(&disable_str, " ---");
	obs_property_list_add_string(p, disable_str.array, disable_str.array);
	dstr_free(&disable_str);

	// A list of sources
	obs_scene_enum_items(scene, motion_list_source, (void*)p);
	obs_property_set_modified_callback(p, source_changed);

	// Toggle for providing a custom start position
	p = obs_properties_add_bool(props, S_START_POS, T_START_POS);
	obs_property_set_modified_callback(p, provide_start_position_toggle_changed);
	// Custom starting X and Y values
	obs_properties_add_int(props, S_ORG_X, T_ORG_X, 0, 8192, 1);
	obs_properties_add_int(props, S_ORG_Y, T_ORG_Y, 0, 8192, 1);

	// Toggle for providing a custom starting size
	p = obs_properties_add_bool(props, S_START_SCALE, T_START_SCALE);
	obs_property_set_modified_callback(p, provide_start_size_toggle_changed);
	// Custom width and height
	obs_properties_add_int(props, S_ORG_W, T_ORG_W, 0, 8192, 1);
	obs_properties_add_int(props, S_ORG_H, T_ORG_H, 0, 8192, 1);

	// Various animation types
	p = obs_properties_add_list(props, S_PATH_TYPE, T_PATH_TYPE,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, T_PATH_LINEAR, S_PATH_LINEAR);
	obs_property_list_add_int(p, T_PATH_QUADRATIC, S_PATH_QUADRATIC);
	obs_property_list_add_int(p, T_PATH_CUBIC, S_PATH_CUBIC);
	obs_property_set_modified_callback(p, path_type_changed);

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

	// Toggle for providing a custom size for the source at its destination
	p = obs_properties_add_bool(props, S_USE_DST_SCALE, T_USE_DST_SCALE);
	obs_property_set_modified_callback(p, 
		provide_custom_size_at_destination_toggle_changed);
	// Custom width and height
	obs_properties_add_int(props, S_DST_W, T_DST_W, 0, 8192, 1);
	obs_properties_add_int(props, S_DST_H, T_DST_H, 0, 8192, 1);

	// Animation duration slider
	obs_properties_add_float_slider(props, S_DURATION, T_DURATION, 0, 5, 
		0.1);

	// Forwards / Backwards button(s)
	obs_properties_add_button(props, S_FORWARD, T_FORWARD, forward_clicked);

	if (filter->round_trip)
		obs_properties_add_button(props, S_BACKWARD, T_BACKWARD, backward_clicked);

	return props;
}

static void motion_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, S_IS_REVERSED, false);
	obs_data_set_default_int(settings, S_SOURCE, -1);
	obs_data_set_default_int(settings, S_ORG_W, 300);
	obs_data_set_default_int(settings, S_ORG_H, 300);
	obs_data_set_default_int(settings, S_DST_W, 300);
	obs_data_set_default_int(settings, S_DST_H, 300);
	obs_data_set_default_double(settings, S_DURATION, 1.0);
}

static void cal_pos(struct motion_filter_data *filter)
{
	float elapsed_time = fmin(filter->duration, filter->elapsed_time);
	float t, p;

	if (filter->motion_reverse){
		p = elapsed_time / filter->duration;
		t = 1.0f - p;
	}
	else {
		t = elapsed_time / filter->duration;
		p = 1.0f - t;
	}

	if (filter->path_type == S_PATH_QUADRATIC){
		filter->position.x = 
			p * p * filter->org_pos.x +
			2 * p * t * filter->ctrl_pos.x +
			t * t * filter->dst_pos.x;
		filter->position.y = 
			p * p * filter->org_pos.y +
			2 * p * t * filter->ctrl_pos.y +
			t * t * filter->dst_pos.y;

	}
	else if (filter->path_type == S_PATH_CUBIC){
		filter->position.x = 
			p * p * p * filter->org_pos.x +
			3 * p * p * t * filter->ctrl_pos.x +
			3 * p * t * t * filter->ctrl2_pos.x +
			t * t * t * filter->dst_pos.x;
		filter->position.y = 
			p * p * p * filter->org_pos.y +
			3 * p * p * t * filter->ctrl_pos.y +
			3 * p * t * t * filter->ctrl2_pos.y +
			t * t * t * filter->dst_pos.y;
	}
	else{
		filter->position.x = p * filter->org_pos.x + t * filter->dst_pos.x;
		filter->position.y = p * filter->org_pos.y + t * filter->dst_pos.y;
	}

	if (filter->use_dst_scale){
		filter->scale.x = p *filter->org_scale.x + t * filter->dst_scale.x;
		filter->scale.y = p *filter->org_scale.y + t * filter->dst_scale.y;
	}
	else{
		filter->scale.x = filter->org_scale.x;
		filter->scale.y = filter->org_scale.y;
	}
}

static void motion_filter_tick(void *data, float seconds)
{
	struct motion_filter_data *filter = data;
	if (filter->motion_start){

		if (filter->duration > 0){
			cal_pos(filter);
			obs_sceneitem_set_pos(filter->item, &filter->position);
			obs_sceneitem_set_scale(filter->item, &filter->scale);
		}
		else{
			obs_sceneitem_set_pos(filter->item, &filter->dst_pos);
			obs_sceneitem_set_scale(filter->item, &filter->dst_scale);
		}

		if (filter->elapsed_time >= filter->duration){
			filter->motion_start = false;
			filter->elapsed_time = 0.0f;
			obs_sceneitem_release(filter->item);
			if (filter->round_trip){
				filter->motion_reverse = !filter->motion_reverse;
				set_reverse_info(filter);
			}
		}
		else
			filter->elapsed_time += seconds;
	}

	if (!filter->hotkey_init)
		init_hotkey(data);
}

static void motion_filter_save(void *data, obs_data_t *settings)
{
	struct motion_filter_data *filter = data;
	obs_data_array_t* array_f = obs_hotkey_save(filter->hotkey_id_f);
	obs_data_array_t* array_b = obs_hotkey_save(filter->hotkey_id_b);
	obs_data_set_array(settings, S_FORWARD, array_f);
	obs_data_set_array(settings, S_BACKWARD, array_b);
	obs_data_array_release(array_f);
	obs_data_array_release(array_b);
}

static void *motion_filter_create(obs_data_t *settings, obs_source_t *context)
{
	struct motion_filter_data *filter = bzalloc(sizeof(*filter));
	
	filter->context = context;
	filter->motion_start = false;
	filter->hotkey_init = false;
	filter->round_trip = false;
	filter->motion_reverse = obs_data_get_bool(settings, S_IS_REVERSED);
	filter->restart_backward = filter->motion_reverse;
	obs_source_update(context, settings);
	return filter;
}

static void *motion_filter_round_create(obs_data_t *settings, 
	obs_source_t *context)
{
	struct motion_filter_data *filter = bzalloc(sizeof(*filter));

	filter->context = context;
	filter->motion_start = false;
	filter->hotkey_init = false;
	filter->round_trip = true;
	filter->motion_reverse = obs_data_get_bool(settings, S_IS_REVERSED);
	filter->restart_backward = filter->motion_reverse;
	obs_source_update(context, settings);
	return filter;
}

static void motion_filter_remove(void *data, obs_source_t *source)
{
	struct motion_filter_data *filter = data;
	if (filter->motion_reverse){
		obs_sceneitem_set_pos(filter->item, &filter->org_pos);
		obs_sceneitem_set_scale(filter->item, &filter->org_scale);
		filter->motion_reverse = false;
	}
	UNUSED_PARAMETER(source);
}

static void motion_filter_destroy(void *data)
{
	struct motion_filter_data *filter = data;
	if (filter->hotkey_id_f)
		obs_hotkey_unregister(filter->hotkey_id_f);

	if (filter->hotkey_id_b && filter->round_trip)
		obs_hotkey_unregister(filter->hotkey_id_b);

	bfree(filter->item_name);
	bfree(filter);
}

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("motion-filter", "en-US")

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

struct obs_source_info round_trip_motion_filter = {
	.id = "round-motion-filter",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = motion_filter_round_get_name,
	.create = motion_filter_round_create,
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
	obs_register_source(&round_trip_motion_filter);
	return true;
}
