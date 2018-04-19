#include <obs-module.h>
#include <obs-hotkey.h>

#define	N_PATH_LINEAR		0
#define N_PATH_QUADRATIC	1
#define N_PATH_CUBIC		2

#define S_IS_REVERSED		"is_revesed"
#define S_ORG_X				"org_x"
#define S_ORG_Y				"org_y"
#define S_ORG_SCALE_X		"org_scale_x"
#define S_ORG_SCALE_Y		"org_scale_y"
#define S_PATH_TYPE			"path_type"
#define S_CTRL_X			"ctrl_x"
#define S_CTRL_Y			"ctrl_y"
#define S_CTRL2_X			"ctrl2_x"
#define S_CTRL2_Y			"ctrl2_y"
#define S_DST_X				"dst_x"
#define S_DST_Y				"dst_y"
#define S_DST_W				"dst_width"
#define S_DST_H				"dst_height"
#define	S_DURATION			"duration"
#define S_SOURCE			"source_id"
#define S_FORWARD			"forward"
#define S_BACKWARD			"backward"

#define T_(v)               obs_module_text(v)
#define T_PATH_TYPE			T_("PathType")
#define	T_PATH_LINEAR		T_("PathType.Linear")
#define T_PATH_QUADRATIC	T_("PathType.Quadratic")
#define T_PATH_CUBIC		T_("PathType.Cubic")
#define T_CTRL_X			T_("ControlPoint.X")
#define T_CTRL_Y			T_("ControlPoint.Y")
#define T_CTRL2_X			T_("ControlPoint2.X")
#define T_CTRL2_Y			T_("ControlPoint2.Y")
#define T_DST_X				T_("Destination.X")
#define T_DST_Y				T_("Destination.Y")
#define T_DST_W				T_("Destination.W")
#define T_DST_H				T_("Destination.H")
#define T_DURATION			T_("Duration")
#define T_SOURCE			T_("SourceName")
#define T_FORWARD			T_("Forward")
#define T_BACKWARD			T_("Backward")

struct motion_filter_data {
	obs_source_t		*context;
	obs_scene_t			*scene;
	obs_sceneitem_t		*item;
	obs_hotkey_pair_id  hotkey_pair;
	bool				hotkey_init;
	bool				restart_backward;
	bool				motion_start;
	bool				motion_reverse;
	int					path_type;
	int					dst_width;
	int					dst_height;
	struct vec2			org_pos;
	struct vec2			ctrl_pos;
	struct vec2			ctrl2_pos;
	struct vec2			dst_pos;
	struct vec2			org_scale;
	struct vec2			dst_scale;
	struct vec2			position;
	struct vec2			scale;
	float				duration;
	float				elapsed_time;
	int64_t				item_id;

};

static inline obs_sceneitem_t* get_item(obs_source_t* context,int64_t id)
{
	obs_source_t* source = obs_filter_get_parent(context);
	obs_scene_t* scene = obs_scene_from_source(source);
	return obs_scene_find_sceneitem_by_id(scene, id);
}

static bool cal_scale(obs_sceneitem_t* item, float* sx, float*sy,
	int width, int height)
{
	obs_source_t* item_source = obs_sceneitem_get_source(item);
	int base_width = obs_source_get_base_width(item_source);
	int base_height = obs_source_get_base_height(item_source);

	if (base_width == 0 || base_height == 0)
		return false;

	*sx = (float)width / base_width;
	*sy = (float)height / base_height;

	return true;
}

static const char *motion_filter_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return T_("Motion");
}

static bool motion_init(void *data, bool forward)
{
	struct motion_filter_data *filter = data;
	filter->item = get_item(filter->context, filter->item_id);

	if (filter->item){
		struct obs_transform_info info;

		if (!cal_scale(filter->item, &filter->dst_scale.x,
			&filter->dst_scale.y, filter->dst_width, filter->dst_height))
			return false;
		else
			obs_sceneitem_get_info(filter->item, &info);

		if (filter->motion_reverse == forward)
			return false;
		else if (!filter->motion_reverse){
			filter->org_pos.x = info.pos.x;
			filter->org_pos.y = info.pos.y;
			filter->org_scale.x = info.scale.x;
			filter->org_scale.y = info.scale.y;
		}

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

static void motion_filter_update(void *data, obs_data_t *settings)
{
	struct motion_filter_data *filter = data;
	struct vec2	pos;
	int width;
	int height;
	int item_id;

	filter->path_type = (int)obs_data_get_int(settings, S_PATH_TYPE);
	filter->ctrl_pos.x = (float)obs_data_get_int(settings, S_CTRL_X);
	filter->ctrl_pos.y = (float)obs_data_get_int(settings, S_CTRL_Y);
	filter->ctrl2_pos.x = (float)obs_data_get_int(settings, S_CTRL2_X);
	filter->ctrl2_pos.y = (float)obs_data_get_int(settings, S_CTRL2_Y);
	filter->duration = (float)obs_data_get_double(settings, S_DURATION);
	item_id = (int)obs_data_get_int(settings, S_SOURCE);
	pos.x = (float)obs_data_get_int(settings, S_DST_X);
	pos.y = (float)obs_data_get_int(settings, S_DST_Y);
	width = (int)obs_data_get_int(settings, S_DST_W);
	height = (int)obs_data_get_int(settings, S_DST_H);

	if (filter->motion_reverse){

		obs_sceneitem_t* item = get_item(filter->context, item_id);

		if (filter->restart_backward){
			obs_sceneitem_set_pos(item, &filter->org_pos);
			obs_sceneitem_set_scale(item, &filter->org_scale);
			filter->motion_reverse = false;
			filter->restart_backward = false;
		}
		else if (item_id != filter->item_id){
			obs_sceneitem_set_pos(filter->item, &filter->org_pos);
			obs_sceneitem_set_scale(filter->item, &filter->org_scale);
			filter->motion_reverse = false;
		} 
		else if (pos.x != filter->dst_pos.x || pos.y != filter->dst_pos.y){
			obs_sceneitem_set_pos(item, &pos);
		} 
		else if (width != filter->dst_width || height != filter->dst_height){
			struct vec2	scale;
			if (cal_scale(item, &scale.x, &scale.y, width, height))
				obs_sceneitem_set_scale(item, &scale);
		}
	}

	filter->dst_pos.x = pos.x;
	filter->dst_pos.y = pos.y;
	filter->dst_width = width;
	filter->dst_height = height;
	filter->item_id = item_id;
}

static bool motion_init_hot_key(void *data)
{
	struct motion_filter_data *filter = data;
	const char* name = obs_source_get_name(filter->context);
	obs_source_t* source = obs_filter_get_parent(filter->context);
	obs_scene_t* scene = obs_scene_from_source(source);

	if (scene){
		filter->hotkey_pair = obs_hotkey_pair_register_source(filter->context,
			S_FORWARD, T_FORWARD, S_BACKWARD, T_BACKWARD,
			hotkey_forward, hotkey_backward, filter, filter);
	}

	filter->hotkey_init = true;
	return true;
}

static bool motion_set_button(obs_properties_t *props, obs_property_t *p,
	bool reversed)
{
	obs_property_t* f = obs_properties_get(props, S_FORWARD);
	obs_property_t* b = obs_properties_get(props, S_BACKWARD);
	obs_property_set_visible(f, !reversed);
	obs_property_set_visible(b, reversed);

	UNUSED_PARAMETER(p);
	return true;
}

static bool forward_clicked(obs_properties_t *props, obs_property_t *p,
	void *data)
{
	if (motion_init(data, true))
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
	obs_property_t* f = obs_properties_get(props, S_FORWARD);
	obs_property_t* b = obs_properties_get(props, S_BACKWARD);
	if (obs_property_visible(f) && obs_property_visible(b))
		return motion_set_button(props, p, reversed);
	else
		return motion_set_button(props, p, false);
}

static bool motion_list_source(obs_scene_t* scene,
	obs_sceneitem_t* item, void* p)
{
	obs_property_t* pr = p;
	obs_source_t* source = obs_sceneitem_get_source(item);
	const char* name = obs_source_get_name(source);
	int64_t id = obs_sceneitem_get_id(item);
	obs_property_list_add_int(p, name, id);
	UNUSED_PARAMETER(scene);
	return true;
}

#define set_vis(var, val, type) \
		do { \
			p = obs_properties_get(props, val); \
			obs_property_set_visible(p, var >= type);\
		} while (false)

static bool path_type_changed(obs_properties_t *props, obs_property_t *p,
	obs_data_t *s)
{
	int path_type = (int)obs_data_get_int(s, S_PATH_TYPE);
	set_vis(path_type, S_CTRL_X, N_PATH_QUADRATIC);
	set_vis(path_type, S_CTRL_Y, N_PATH_QUADRATIC);
	set_vis(path_type, S_CTRL2_X, N_PATH_CUBIC);
	set_vis(path_type, S_CTRL2_Y, N_PATH_CUBIC);
	return true;
}

#undef set_vis
static obs_properties_t *motion_filter_properties(void *data)
{
	struct motion_filter_data *filter = data;
	obs_properties_t *props = obs_properties_create();
	obs_property_t* p;

	obs_data_t *settings = obs_source_get_settings(filter->context);

	obs_source_t* source = obs_filter_get_parent(filter->context);
	obs_scene_t* scene = obs_scene_from_source(source);

	if (!scene)
		return props;

	obs_data_set_bool(settings, S_IS_REVERSED, filter->motion_reverse);

	p = obs_properties_add_list(props, S_SOURCE, T_SOURCE,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	
	obs_scene_enum_items(scene, motion_list_source, (void*)p);

	obs_property_set_modified_callback(p, source_changed);

	p = obs_properties_add_list(props, S_PATH_TYPE, T_PATH_TYPE,
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_list_add_int(p, T_PATH_LINEAR, N_PATH_LINEAR);
	obs_property_list_add_int(p, T_PATH_QUADRATIC, N_PATH_QUADRATIC);
	obs_property_list_add_int(p, T_PATH_CUBIC, N_PATH_CUBIC);

	obs_property_set_modified_callback(p, path_type_changed);

	obs_properties_add_int(props, S_DST_X, T_DST_X, -8192, 8192, 1);
	obs_properties_add_int(props, S_DST_Y, T_DST_Y, -8192, 8192, 1);
	obs_properties_add_int(props, S_CTRL_X, T_CTRL_X, -8192, 8192, 1);
	obs_properties_add_int(props, S_CTRL_Y, T_CTRL_Y, -8192, 8192, 1);
	obs_properties_add_int(props, S_CTRL2_X, T_CTRL2_X, -8192, 8192, 1);
	obs_properties_add_int(props, S_CTRL2_Y, T_CTRL2_Y, -8192, 8192, 1);
	obs_properties_add_int(props, S_DST_W, T_DST_W, 0, 8192, 1);
	obs_properties_add_int(props, S_DST_H, T_DST_H, 0, 8192, 1);

	obs_properties_add_float_slider(props, S_DURATION, T_DURATION, 0, 5, 
		0.1);

	obs_properties_add_button(props, S_FORWARD, T_FORWARD, forward_clicked);
	obs_properties_add_button(props, S_BACKWARD, T_BACKWARD, backward_clicked);

	return props;
}

static void motion_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_bool(settings, S_IS_REVERSED, false);
	obs_data_set_default_int(settings, S_DST_W, 300);
	obs_data_set_default_int(settings, S_DST_H, 300);
	obs_data_set_default_double(settings, S_DURATION, 1.0);
}

static void motion_cal_pos(struct motion_filter_data *filter)
{
	float elapsed_time = min(filter->duration, filter->elapsed_time);
	float t, p;

	if (filter->motion_reverse){
		p = elapsed_time / filter->duration;
		t = 1.0f - p;
	}
	else {
		t = elapsed_time / filter->duration;
		p = 1.0f - t;
	}

	if (filter->path_type == N_PATH_QUADRATIC){
		filter->position.x = 
			p * p * filter->org_pos.x +
			2 * p * t * filter->ctrl_pos.x +
			t * t * filter->dst_pos.x;
		filter->position.y = 
			p * p * filter->org_pos.y +
			2 * p * t * filter->ctrl_pos.y +
			t * t * filter->dst_pos.y;

	}
	else if (filter->path_type == N_PATH_CUBIC){
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

	filter->scale.x = p *filter->org_scale.x + t * filter->dst_scale.x;
	filter->scale.y = p *filter->org_scale.y + t * filter->dst_scale.y;
}

static void motion_filter_tick(void *data, float seconds)
{
	struct motion_filter_data *filter = data;
	if (filter->motion_start){

		if (filter->duration > 0){
			motion_cal_pos(filter);
			obs_sceneitem_set_pos(filter->item, &filter->position);
			obs_sceneitem_set_scale(filter->item, &filter->scale);
		}
		else{
			obs_sceneitem_set_pos(filter->item, &filter->dst_pos);
			obs_sceneitem_set_scale(filter->item, &filter->dst_scale);
		}

		if (filter->elapsed_time >= filter->duration){
			filter->motion_start = false;
			filter->motion_reverse = !filter->motion_reverse;
			filter->elapsed_time = 0.0f;
		}
		else{
			filter->elapsed_time += seconds;
		}
	}

	if (!filter->hotkey_init)
		motion_init_hot_key(data);
}

static void motion_filter_save(void *data, obs_data_t *settings)
{
	struct motion_filter_data *filter = data;
	obs_data_set_bool(settings, S_IS_REVERSED, filter->motion_reverse);
	if (filter->motion_reverse){
		obs_data_set_double(settings, S_ORG_X, filter->org_pos.x);
		obs_data_set_double(settings, S_ORG_Y, filter->org_pos.y);
		obs_data_set_double(settings, S_ORG_SCALE_X, filter->org_scale.x);
		obs_data_set_double(settings, S_ORG_SCALE_Y, filter->org_scale.y);
	}
}

static void *motion_filter_create(obs_data_t *settings, obs_source_t *context)
{
	struct motion_filter_data *filter = bzalloc(sizeof(*filter));
	
	filter->context = context;
	filter->motion_start = false;
	filter->hotkey_init = false;
	filter->motion_reverse = obs_data_get_bool(settings, S_IS_REVERSED);
	filter->restart_backward = filter->motion_reverse;
	if (filter->motion_reverse){
		filter->org_pos.x = (float)obs_data_get_double(settings, S_ORG_X);
		filter->org_pos.y = (float)obs_data_get_double(settings, S_ORG_Y);
		filter->org_scale.x = (float)obs_data_get_double(settings,
			S_ORG_SCALE_X);
		filter->org_scale.y = (float)obs_data_get_double(settings,
			S_ORG_SCALE_Y);
	}

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
	if (filter->hotkey_pair){
		obs_hotkey_pair_unregister(filter->hotkey_pair);
	}
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

bool obs_module_load(void) {
	obs_register_source(&motion_filter);
	return true;
}
