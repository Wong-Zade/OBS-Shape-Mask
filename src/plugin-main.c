#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/math-defs.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-shape-window", "en-US")

#define SETTING_SHAPE    "shape_type"
#define SETTING_FEATHER  "feather"
#define SETTING_SCALE    "shape_scale"
#define SETTING_OFFSET_X "offset_x"
#define SETTING_OFFSET_Y "offset_y"

enum shape_type {
	SHAPE_CIRCLE = 0,
	SHAPE_ROUNDED_RECT = 1,
	SHAPE_ELLIPSE = 2,
};

struct shape_mask_filter {
	obs_source_t *source;
	gs_effect_t *effect;

	gs_eparam_t *param_shape_type;
	gs_eparam_t *param_feather;
	gs_eparam_t *param_scale;
	gs_eparam_t *param_offset;
	gs_eparam_t *param_uv_size;

	int shape;
	float feather;
	float scale;
	float offset_x;
	float offset_y;
};

static const char *shape_mask_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("ShapeMask.Name");
}

static void shape_mask_update(void *data, obs_data_t *settings)
{
	struct shape_mask_filter *f = data;

	f->shape = (int)obs_data_get_int(settings, SETTING_SHAPE);
	f->feather = (float)obs_data_get_double(settings, SETTING_FEATHER);
	f->scale = (float)obs_data_get_double(settings, SETTING_SCALE);
	f->offset_x = (float)obs_data_get_double(settings, SETTING_OFFSET_X);
	f->offset_y = (float)obs_data_get_double(settings, SETTING_OFFSET_Y);
}

static void *shape_mask_create(obs_data_t *settings, obs_source_t *source)
{
	struct shape_mask_filter *f = bzalloc(sizeof(struct shape_mask_filter));
	f->source = source;

	char *effect_path = obs_module_file("shape_mask.effect");

	obs_enter_graphics();
	f->effect = gs_effect_create_from_file(effect_path, NULL);
	obs_leave_graphics();

	bfree(effect_path);

	if (!f->effect) {
		blog(LOG_ERROR, "[obs-shape-window] Failed to load shape_mask.effect");
		bfree(f);
		return NULL;
	}

	f->param_shape_type = gs_effect_get_param_by_name(f->effect, "shape_type");
	f->param_feather = gs_effect_get_param_by_name(f->effect, "feather");
	f->param_scale = gs_effect_get_param_by_name(f->effect, "shape_scale");
	f->param_offset = gs_effect_get_param_by_name(f->effect, "offset");
	f->param_uv_size = gs_effect_get_param_by_name(f->effect, "uv_size");

	shape_mask_update(f, settings);
	return f;
}

static void shape_mask_destroy(void *data)
{
	struct shape_mask_filter *f = data;

	obs_enter_graphics();
	gs_effect_destroy(f->effect);
	obs_leave_graphics();

	bfree(f);
}

static uint32_t shape_mask_width(void *data)
{
	struct shape_mask_filter *f = data;
	obs_source_t *target = obs_filter_get_target(f->source);
	return target ? obs_source_get_base_width(target) : 0;
}

static uint32_t shape_mask_height(void *data)
{
	struct shape_mask_filter *f = data;
	obs_source_t *target = obs_filter_get_target(f->source);
	return target ? obs_source_get_base_height(target) : 0;
}

static void shape_mask_video_render(void *data, gs_effect_t *unused_effect)
{
	UNUSED_PARAMETER(unused_effect);
	struct shape_mask_filter *f = data;

	if (!f->effect)
		return;

	obs_source_t *target = obs_filter_get_target(f->source);
	obs_source_t *parent = obs_filter_get_parent(f->source);
	if (!target || !parent)
		return;

	uint32_t width = obs_source_get_base_width(target);
	uint32_t height = obs_source_get_base_height(target);
	if (!width || !height)
		return;

	if (!obs_source_process_filter_begin(f->source, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING))
		return;

	struct vec2 uv_size;
	vec2_set(&uv_size, (float)width, (float)height);

	struct vec2 offset;
	vec2_set(&offset, f->offset_x, f->offset_y);

	gs_effect_set_int(f->param_shape_type, f->shape);
	gs_effect_set_float(f->param_feather, f->feather);
	gs_effect_set_float(f->param_scale, f->scale);
	gs_effect_set_vec2(f->param_offset, &offset);
	gs_effect_set_vec2(f->param_uv_size, &uv_size);

	obs_source_process_filter_end(f->source, f->effect, width, height);
}

static obs_properties_t *shape_mask_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *props = obs_properties_create();

	obs_property_t *shape_list = obs_properties_add_list(props, SETTING_SHAPE, obs_module_text("ShapeMask.Shape"),
							       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_list_add_int(shape_list, obs_module_text("ShapeMask.Shape.Circle"), SHAPE_CIRCLE);
	obs_property_list_add_int(shape_list, obs_module_text("ShapeMask.Shape.RoundedRect"), SHAPE_ROUNDED_RECT);
	obs_property_list_add_int(shape_list, obs_module_text("ShapeMask.Shape.Ellipse"), SHAPE_ELLIPSE);

	obs_properties_add_float_slider(props, SETTING_SCALE, obs_module_text("ShapeMask.Scale"), 0.1, 1.5, 0.01);
	obs_properties_add_float_slider(props, SETTING_FEATHER, obs_module_text("ShapeMask.Feather"), 0.0, 0.25, 0.005);
	obs_properties_add_float_slider(props, SETTING_OFFSET_X, obs_module_text("ShapeMask.OffsetX"), -0.5, 0.5, 0.01);
	obs_properties_add_float_slider(props, SETTING_OFFSET_Y, obs_module_text("ShapeMask.OffsetY"), -0.5, 0.5, 0.01);

	return props;
}

static void shape_mask_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, SETTING_SHAPE, SHAPE_CIRCLE);
	obs_data_set_default_double(settings, SETTING_SCALE, 1.0);
	obs_data_set_default_double(settings, SETTING_FEATHER, 0.02);
	obs_data_set_default_double(settings, SETTING_OFFSET_X, 0.0);
	obs_data_set_default_double(settings, SETTING_OFFSET_Y, 0.0);
}

struct obs_source_info shape_mask_filter_info = {
	.id = "obs_shape_window",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = shape_mask_get_name,
	.create = shape_mask_create,
	.destroy = shape_mask_destroy,
	.update = shape_mask_update,
	.video_render = shape_mask_video_render,
	.get_width = shape_mask_width,
	.get_height = shape_mask_height,
	.get_properties = shape_mask_properties,
	.get_defaults = shape_mask_defaults,
};

bool obs_module_load(void)
{
	obs_register_source(&shape_mask_filter_info);
	blog(LOG_INFO, "[obs-shape-window] plugin loaded (version 0.1.0)");
	return true;
}

void obs_module_unload(void)
{
}
