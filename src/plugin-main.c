#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/math-defs.h>
#include <string.h>
#include <math.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-shape-window", "en-US")

#define SETTING_SHAPE         "shape_type"
#define SETTING_SCALE_X       "scale_x"
#define SETTING_SCALE_Y       "scale_y"
#define SETTING_OFFSET_X      "offset_x"
#define SETTING_OFFSET_Y      "offset_y"
#define SETTING_FEATHER       "feather"
#define SETTING_CORNER_RADIUS "corner_radius"
#define SETTING_ROTATION      "rotation"
#define SETTING_MASK_INVERT   "mask_invert"
#define SETTING_MASK_MODE     "mask_mode"
#define SETTING_MASK_SOURCE   "mask_source_name"

#define SETTING_BORDER_ENABLE "border_enabled"
#define SETTING_BORDER_COLOR  "border_color"
#define SETTING_BORDER_WIDTH  "border_width"
#define SETTING_BORDER_SOFT   "border_softness"
#define SETTING_BORDER_OPACITY "border_opacity"
#define SETTING_BORDER_GLOW   "border_glow"
#define SETTING_BORDER_PULSE  "border_pulse"
#define SETTING_PULSE_SPEED   "pulse_speed"
#define SETTING_PULSE_AMOUNT  "pulse_amount"

#define SETTING_SHADOW_ENABLE  "shadow_enabled"
#define SETTING_SHADOW_COLOR   "shadow_color"
#define SETTING_SHADOW_OFFSETX "shadow_offset_x"
#define SETTING_SHADOW_OFFSETY "shadow_offset_y"
#define SETTING_SHADOW_BLUR    "shadow_blur"
#define SETTING_SHADOW_OPACITY "shadow_opacity"

#define SETTING_ANIMATION      "animation"
#define SETTING_ANIM_SPEED     "animation_speed"
#define SETTING_ANIM_AMOUNT    "animation_amount"

#define HOTKEY_POP_ANIM "shape_mask.trigger_pop"
#define ANIM_DURATION 0.40f

enum shape_type {
	SHAPE_CIRCLE = 0,
	SHAPE_ROUNDED_RECT = 1,
	SHAPE_RECTANGLE = 2,
};

enum animation_type {
	ANIM_NONE = 0,
	ANIM_PULSE = 1,
	ANIM_BREATHE = 2,
	ANIM_ROTATE = 3,
	ANIM_BOUNCE = 4,
	ANIM_SHAKE = 5,
	ANIM_SCALE = 6,
};

enum mask_mode {
	MASK_MODE_SHAPE = 0,
	MASK_MODE_SOURCE = 1,
};

struct shape_mask_filter {
	obs_source_t *source;
	gs_effect_t *effect;

	gs_eparam_t *param_shape_type;
	gs_eparam_t *param_scale;
	gs_eparam_t *param_offset;
	gs_eparam_t *param_uv_size;
	gs_eparam_t *param_feather;
	gs_eparam_t *param_corner_radius;
	gs_eparam_t *param_rotation;
	gs_eparam_t *param_mask_invert;
	gs_eparam_t *param_use_source_mask;
	gs_eparam_t *param_mask_image;

	gs_eparam_t *param_border_enabled;
	gs_eparam_t *param_border_color;
	gs_eparam_t *param_border_width;
	gs_eparam_t *param_border_softness;
	gs_eparam_t *param_border_opacity;
	gs_eparam_t *param_border_glow;
	gs_eparam_t *param_border_pulse;
	gs_eparam_t *param_pulse_speed;
	gs_eparam_t *param_pulse_amount;
	gs_eparam_t *param_time;

	gs_eparam_t *param_shadow_enabled;
	gs_eparam_t *param_shadow_color;
	gs_eparam_t *param_shadow_offset;
	gs_eparam_t *param_shadow_blur;
	gs_eparam_t *param_shadow_opacity;

	gs_eparam_t *param_animation;
	gs_eparam_t *param_animation_speed;
	gs_eparam_t *param_animation_amount;

	int shape;
	float scale_x;
	float scale_y;
	float offset_x;
	float offset_y;
	float feather;
	float corner_radius;
	float rotation;
	bool mask_invert;

	int mask_mode;
	char *mask_source_name;
	obs_weak_source_t *mask_source_weak;
	gs_texrender_t *mask_texrender;

	bool border_enabled;
	uint32_t border_color;
	float border_width;
	float border_softness;
	float border_opacity;
	float border_glow;
	bool border_pulse;
	float pulse_speed;
	float pulse_amount;

	bool shadow_enabled;
	uint32_t shadow_color;
	float shadow_offset_x;
	float shadow_offset_y;
	float shadow_blur;
	float shadow_opacity;

	int animation;
	float animation_speed;
	float animation_amount;

	float elapsed_time;

	obs_hotkey_id anim_hotkey;
	bool anim_active;
	float anim_elapsed;
};

static const char *shape_mask_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("ShapeMask.Name");
}

static float clampf(float v, float lo, float hi)
{
	if (v < lo)
		return lo;
	if (v > hi)
		return hi;
	return v;
}

static float ease_out_back(float t)
{
	const float c1 = 1.70158f;
	const float c3 = c1 + 1.0f;
	float u = t - 1.0f;
	return 1.0f + c3 * u * u * u + c1 * u * u;
}

static void shape_mask_trigger_pop(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	struct shape_mask_filter *f = data;
	if (!pressed)
		return;

	f->anim_active = true;
	f->anim_elapsed = 0.0f;
}

static void shape_mask_update_mask_source(struct shape_mask_filter *f, const char *name)
{
	if (f->mask_source_weak) {
		obs_weak_source_release(f->mask_source_weak);
		f->mask_source_weak = NULL;
	}

	if (!name || !*name)
		return;

	obs_source_t *src = obs_get_source_by_name(name);
	if (src) {
		f->mask_source_weak = obs_source_get_weak_source(src);
		obs_source_release(src);
	}
}

static void shape_mask_update(void *data, obs_data_t *settings)
{
	struct shape_mask_filter *f = data;

	f->shape = (int)obs_data_get_int(settings, SETTING_SHAPE);
	f->scale_x = (float)obs_data_get_double(settings, SETTING_SCALE_X);
	f->scale_y = (float)obs_data_get_double(settings, SETTING_SCALE_Y);
	f->offset_x = (float)obs_data_get_double(settings, SETTING_OFFSET_X);
	f->offset_y = (float)obs_data_get_double(settings, SETTING_OFFSET_Y);
	f->feather = (float)obs_data_get_double(settings, SETTING_FEATHER);
	f->corner_radius = (float)obs_data_get_double(settings, SETTING_CORNER_RADIUS);
	f->rotation = (float)obs_data_get_double(settings, SETTING_ROTATION);
	f->mask_invert = obs_data_get_bool(settings, SETTING_MASK_INVERT);
	f->mask_mode = (int)obs_data_get_int(settings, SETTING_MASK_MODE);

	const char *mask_source_name = obs_data_get_string(settings, SETTING_MASK_SOURCE);
	if (!f->mask_source_name || strcmp(f->mask_source_name, mask_source_name) != 0) {
		bfree(f->mask_source_name);
		f->mask_source_name = bstrdup(mask_source_name);
		shape_mask_update_mask_source(f, mask_source_name);
	}

	f->border_enabled = obs_data_get_bool(settings, SETTING_BORDER_ENABLE);
	f->border_color = (uint32_t)obs_data_get_int(settings, SETTING_BORDER_COLOR);
	f->border_width = (float)obs_data_get_double(settings, SETTING_BORDER_WIDTH);
	f->border_softness = (float)obs_data_get_double(settings, SETTING_BORDER_SOFT);
	f->border_opacity = (float)obs_data_get_double(settings, SETTING_BORDER_OPACITY);
	f->border_glow = (float)obs_data_get_double(settings, SETTING_BORDER_GLOW);
	f->border_pulse = obs_data_get_bool(settings, SETTING_BORDER_PULSE);
	f->pulse_speed = (float)obs_data_get_double(settings, SETTING_PULSE_SPEED);
	f->pulse_amount = (float)obs_data_get_double(settings, SETTING_PULSE_AMOUNT);

	f->shadow_enabled = obs_data_get_bool(settings, SETTING_SHADOW_ENABLE);
	f->shadow_color = (uint32_t)obs_data_get_int(settings, SETTING_SHADOW_COLOR);
	f->shadow_offset_x = (float)obs_data_get_double(settings, SETTING_SHADOW_OFFSETX);
	f->shadow_offset_y = (float)obs_data_get_double(settings, SETTING_SHADOW_OFFSETY);
	f->shadow_blur = (float)obs_data_get_double(settings, SETTING_SHADOW_BLUR);
	f->shadow_opacity = (float)obs_data_get_double(settings, SETTING_SHADOW_OPACITY);

	f->animation = (int)obs_data_get_int(settings, SETTING_ANIMATION);
	f->animation_speed = (float)obs_data_get_double(settings, SETTING_ANIM_SPEED);
	f->animation_amount = (float)obs_data_get_double(settings, SETTING_ANIM_AMOUNT);

	f->scale_x = clampf(f->scale_x, 0.05f, 1.5f);
	f->scale_y = clampf(f->scale_y, 0.05f, 1.5f);
	f->feather = clampf(f->feather, 0.0f, 0.20f);
	f->corner_radius = clampf(f->corner_radius, 0.0f, 0.50f);
	f->rotation = clampf(f->rotation, -180.0f, 180.0f);
	f->border_width = clampf(f->border_width, 0.0f, 0.12f);
	f->border_softness = clampf(f->border_softness, 0.0f, 0.05f);
	f->border_opacity = clampf(f->border_opacity, 0.0f, 1.0f);
	f->border_glow = clampf(f->border_glow, 0.0f, 1.0f);
	f->pulse_speed = clampf(f->pulse_speed, 0.0f, 12.0f);
	f->pulse_amount = clampf(f->pulse_amount, 0.0f, 1.0f);
	f->shadow_offset_x = clampf(f->shadow_offset_x, -0.3f, 0.3f);
	f->shadow_offset_y = clampf(f->shadow_offset_y, -0.3f, 0.3f);
	f->shadow_blur = clampf(f->shadow_blur, 0.001f, 0.25f);
	f->shadow_opacity = clampf(f->shadow_opacity, 0.0f, 1.0f);
	f->animation_speed = clampf(f->animation_speed, 0.0f, 10.0f);
	f->animation_amount = clampf(f->animation_amount, 0.0f, 1.0f);
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
	f->param_scale = gs_effect_get_param_by_name(f->effect, "shape_scale");
	f->param_offset = gs_effect_get_param_by_name(f->effect, "offset");
	f->param_uv_size = gs_effect_get_param_by_name(f->effect, "uv_size");
	f->param_feather = gs_effect_get_param_by_name(f->effect, "feather");
	f->param_corner_radius = gs_effect_get_param_by_name(f->effect, "corner_radius");
	f->param_rotation = gs_effect_get_param_by_name(f->effect, "rotation");
	f->param_mask_invert = gs_effect_get_param_by_name(f->effect, "mask_invert");
	f->param_use_source_mask = gs_effect_get_param_by_name(f->effect, "use_source_mask");
	f->param_mask_image = gs_effect_get_param_by_name(f->effect, "mask_image");

	f->param_border_enabled = gs_effect_get_param_by_name(f->effect, "border_enabled");
	f->param_border_color = gs_effect_get_param_by_name(f->effect, "border_color");
	f->param_border_width = gs_effect_get_param_by_name(f->effect, "border_width");
	f->param_border_softness = gs_effect_get_param_by_name(f->effect, "border_softness");
	f->param_border_opacity = gs_effect_get_param_by_name(f->effect, "border_opacity");
	f->param_border_glow = gs_effect_get_param_by_name(f->effect, "border_glow");
	f->param_border_pulse = gs_effect_get_param_by_name(f->effect, "border_pulse");
	f->param_pulse_speed = gs_effect_get_param_by_name(f->effect, "pulse_speed");
	f->param_pulse_amount = gs_effect_get_param_by_name(f->effect, "pulse_amount");
	f->param_time = gs_effect_get_param_by_name(f->effect, "time_seconds");

	f->param_shadow_enabled = gs_effect_get_param_by_name(f->effect, "shadow_enabled");
	f->param_shadow_color = gs_effect_get_param_by_name(f->effect, "shadow_color");
	f->param_shadow_offset = gs_effect_get_param_by_name(f->effect, "shadow_offset");
	f->param_shadow_blur = gs_effect_get_param_by_name(f->effect, "shadow_blur");
	f->param_shadow_opacity = gs_effect_get_param_by_name(f->effect, "shadow_opacity");

	f->param_animation = gs_effect_get_param_by_name(f->effect, "animation");
	f->param_animation_speed = gs_effect_get_param_by_name(f->effect, "animation_speed");
	f->param_animation_amount = gs_effect_get_param_by_name(f->effect, "animation_amount");

	f->anim_hotkey = obs_hotkey_register_source(source, HOTKEY_POP_ANIM, obs_module_text("ShapeMask.PopHotkey"),
						      shape_mask_trigger_pop, f);

	shape_mask_update(f, settings);
	return f;
}

static void shape_mask_destroy(void *data)
{
	struct shape_mask_filter *f = data;

	obs_hotkey_unregister(f->anim_hotkey);

	if (f->mask_source_weak)
		obs_weak_source_release(f->mask_source_weak);

	bfree(f->mask_source_name);

	obs_enter_graphics();
	if (f->effect)
		gs_effect_destroy(f->effect);
	if (f->mask_texrender)
		gs_texrender_destroy(f->mask_texrender);
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

static void shape_mask_video_tick(void *data, float seconds)
{
	struct shape_mask_filter *f = data;

	/* time_seconds drives border pulse and the general animation system
	   (bounce/shake/pulse/breathe/scale) -- advance the clock if either
	   is active. */
	bool needs_clock = f->border_pulse || f->animation != ANIM_NONE;

	if (needs_clock) {
		f->elapsed_time += seconds;
		if (f->elapsed_time > 10000.0f)
			f->elapsed_time -= 10000.0f;
	}

	if (f->anim_active) {
		f->anim_elapsed += seconds;
		if (f->anim_elapsed >= ANIM_DURATION) {
			f->anim_active = false;
			f->anim_elapsed = ANIM_DURATION;
		}
	}
}

static float shape_mask_get_anim_scale(struct shape_mask_filter *f)
{
	if (!f->anim_active && f->anim_elapsed <= 0.0f)
		return 1.0f;

	float t = clampf(f->anim_elapsed / ANIM_DURATION, 0.0f, 1.0f);
	float eased = ease_out_back(t);
	/* Pop in from 55% size up to (and slightly past) full size. */
	return 0.55f + 0.45f * eased;
}

/* Renders the picked mask source into a private off-screen texture so its
   alpha channel can be sampled as a mask by the main shader. Guards against
   picking the filter's own parent (which would recurse into itself). */
static void shape_mask_render_source_mask(struct shape_mask_filter *f)
{
	if (!f->mask_source_weak)
		return;

	obs_source_t *mask_source = obs_weak_source_get_source(f->mask_source_weak);
	if (!mask_source)
		return;

	obs_source_t *parent = obs_filter_get_parent(f->source);
	if (mask_source == parent) {
		/* Picking the source this filter is attached to would render
		   it recursively. Skip rendering this frame. */
		obs_source_release(mask_source);
		return;
	}

	uint32_t mw = obs_source_get_width(mask_source);
	uint32_t mh = obs_source_get_height(mask_source);

	if (mw > 0 && mh > 0) {
		if (!f->mask_texrender)
			f->mask_texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);

		gs_texrender_reset(f->mask_texrender);

		if (gs_texrender_begin(f->mask_texrender, mw, mh)) {
			struct vec4 clear_color;
			vec4_zero(&clear_color);
			gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
			gs_ortho(0.0f, (float)mw, 0.0f, (float)mh, -100.0f, 100.0f);

			obs_source_video_render(mask_source);

			gs_texrender_end(f->mask_texrender);
		}
	}

	obs_source_release(mask_source);
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

	bool use_source_mask = (f->mask_mode == MASK_MODE_SOURCE && f->mask_source_weak != NULL);

	if (use_source_mask)
		shape_mask_render_source_mask(f);

	if (!obs_source_process_filter_begin(f->source, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING))
		return;

	float anim_scale = shape_mask_get_anim_scale(f);

	struct vec2 uv_size;
	vec2_set(&uv_size, (float)width, (float)height);

	struct vec2 offset;
	vec2_set(&offset, f->offset_x, f->offset_y);

	struct vec2 scale;
	vec2_set(&scale, f->scale_x * anim_scale, f->scale_y * anim_scale);

	struct vec4 border_color;
	vec4_from_rgba(&border_color, f->border_color);

	struct vec2 shadow_offset;
	vec2_set(&shadow_offset, f->shadow_offset_x, f->shadow_offset_y);

	struct vec4 shadow_color;
	vec4_from_rgba(&shadow_color, f->shadow_color);

	gs_effect_set_int(f->param_shape_type, f->shape);
	gs_effect_set_vec2(f->param_scale, &scale);
	gs_effect_set_vec2(f->param_offset, &offset);
	gs_effect_set_vec2(f->param_uv_size, &uv_size);
	gs_effect_set_float(f->param_feather, f->feather);
	gs_effect_set_float(f->param_corner_radius, f->corner_radius);
	gs_effect_set_float(f->param_rotation, f->rotation);
	gs_effect_set_bool(f->param_mask_invert, f->mask_invert);
	gs_effect_set_bool(f->param_use_source_mask, use_source_mask);

	if (use_source_mask && f->mask_texrender) {
		gs_texture_t *mask_tex = gs_texrender_get_texture(f->mask_texrender);
		if (mask_tex)
			gs_effect_set_texture(f->param_mask_image, mask_tex);
	}

	/* Border only makes sense against the shape's distance field, which a
	   source mask doesn't have, so it's skipped in that mode. */
	gs_effect_set_bool(f->param_border_enabled, f->border_enabled && !use_source_mask);
	gs_effect_set_vec4(f->param_border_color, &border_color);
	gs_effect_set_float(f->param_border_width, f->border_width);
	gs_effect_set_float(f->param_border_softness, f->border_softness);
	gs_effect_set_float(f->param_border_opacity, f->border_opacity);
	gs_effect_set_float(f->param_border_glow, f->border_glow);
	gs_effect_set_bool(f->param_border_pulse, f->border_pulse);
	gs_effect_set_float(f->param_pulse_speed, f->pulse_speed);
	gs_effect_set_float(f->param_pulse_amount, f->pulse_amount);
	gs_effect_set_float(f->param_time, f->elapsed_time);

	gs_effect_set_bool(f->param_shadow_enabled, f->shadow_enabled);
	gs_effect_set_vec4(f->param_shadow_color, &shadow_color);
	gs_effect_set_vec2(f->param_shadow_offset, &shadow_offset);
	gs_effect_set_float(f->param_shadow_blur, f->shadow_blur);
	gs_effect_set_float(f->param_shadow_opacity, f->shadow_opacity);

	gs_effect_set_int(f->param_animation, f->animation);
	gs_effect_set_float(f->param_animation_speed, f->animation_speed);
	gs_effect_set_float(f->param_animation_amount, f->animation_amount);

	obs_source_process_filter_end(f->source, f->effect, width, height);
}

static bool border_enabled_modified(obs_properties_t *props, obs_property_t *prop, obs_data_t *settings)
{
	UNUSED_PARAMETER(prop);

	bool enabled = obs_data_get_bool(settings, SETTING_BORDER_ENABLE);

	obs_property_set_visible(obs_properties_get(props, SETTING_BORDER_COLOR), enabled);
	obs_property_set_visible(obs_properties_get(props, SETTING_BORDER_WIDTH), enabled);
	obs_property_set_visible(obs_properties_get(props, SETTING_BORDER_SOFT), enabled);
	obs_property_set_visible(obs_properties_get(props, SETTING_BORDER_OPACITY), enabled);
	obs_property_set_visible(obs_properties_get(props, SETTING_BORDER_GLOW), enabled);
	obs_property_set_visible(obs_properties_get(props, SETTING_BORDER_PULSE), enabled);

	bool pulse = obs_data_get_bool(settings, SETTING_BORDER_PULSE);

	obs_property_set_visible(obs_properties_get(props, SETTING_PULSE_SPEED), enabled && pulse);
	obs_property_set_visible(obs_properties_get(props, SETTING_PULSE_AMOUNT), enabled && pulse);

	return true;
}

static bool border_pulse_modified(obs_properties_t *props, obs_property_t *prop, obs_data_t *settings)
{
	UNUSED_PARAMETER(prop);

	bool visible = obs_data_get_bool(settings, SETTING_BORDER_ENABLE) &&
		       obs_data_get_bool(settings, SETTING_BORDER_PULSE);

	obs_property_set_visible(obs_properties_get(props, SETTING_PULSE_SPEED), visible);
	obs_property_set_visible(obs_properties_get(props, SETTING_PULSE_AMOUNT), visible);

	return true;
}

static bool shadow_enabled_modified(obs_properties_t *props, obs_property_t *prop, obs_data_t *settings)
{
	UNUSED_PARAMETER(prop);

	bool enabled = obs_data_get_bool(settings, SETTING_SHADOW_ENABLE);

	obs_property_set_visible(obs_properties_get(props, SETTING_SHADOW_COLOR), enabled);
	obs_property_set_visible(obs_properties_get(props, SETTING_SHADOW_OFFSETX), enabled);
	obs_property_set_visible(obs_properties_get(props, SETTING_SHADOW_OFFSETY), enabled);
	obs_property_set_visible(obs_properties_get(props, SETTING_SHADOW_BLUR), enabled);
	obs_property_set_visible(obs_properties_get(props, SETTING_SHADOW_OPACITY), enabled);

	return true;
}

static bool shape_modified(obs_properties_t *props, obs_property_t *prop, obs_data_t *settings)
{
	UNUSED_PARAMETER(prop);

	int shape = (int)obs_data_get_int(settings, SETTING_SHAPE);

	obs_property_set_visible(obs_properties_get(props, SETTING_CORNER_RADIUS), shape == SHAPE_ROUNDED_RECT);

	return true;
}

static bool mask_mode_modified(obs_properties_t *props, obs_property_t *prop, obs_data_t *settings)
{
	UNUSED_PARAMETER(prop);

	int mode = (int)obs_data_get_int(settings, SETTING_MASK_MODE);
	bool shape_mode = (mode == MASK_MODE_SHAPE);
	bool source_mode = (mode == MASK_MODE_SOURCE);

	obs_property_set_visible(obs_properties_get(props, SETTING_SHAPE), shape_mode);
	obs_property_set_visible(obs_properties_get(props, SETTING_SCALE_X), shape_mode);
	obs_property_set_visible(obs_properties_get(props, SETTING_SCALE_Y), shape_mode);
	obs_property_set_visible(obs_properties_get(props, SETTING_CORNER_RADIUS), shape_mode);
	obs_property_set_visible(obs_properties_get(props, SETTING_BORDER_ENABLE), shape_mode);

	obs_property_set_visible(obs_properties_get(props, SETTING_MASK_SOURCE), source_mode);

	if (shape_mode) {
		shape_modified(props, NULL, settings);
		border_enabled_modified(props, NULL, settings);
	} else {
		/* Source Mask mode: the border ring effect has no distance field
		   to work from, so hide every border-related control regardless
		   of the border_enabled setting underneath. */
		obs_property_set_visible(obs_properties_get(props, SETTING_BORDER_COLOR), false);
		obs_property_set_visible(obs_properties_get(props, SETTING_BORDER_WIDTH), false);
		obs_property_set_visible(obs_properties_get(props, SETTING_BORDER_SOFT), false);
		obs_property_set_visible(obs_properties_get(props, SETTING_BORDER_OPACITY), false);
		obs_property_set_visible(obs_properties_get(props, SETTING_BORDER_GLOW), false);
		obs_property_set_visible(obs_properties_get(props, SETTING_BORDER_PULSE), false);
		obs_property_set_visible(obs_properties_get(props, SETTING_PULSE_SPEED), false);
		obs_property_set_visible(obs_properties_get(props, SETTING_PULSE_AMOUNT), false);
	}

	return true;
}

static bool enum_source_add(void *data, obs_source_t *src)
{
	obs_property_t *list = data;
	uint32_t caps = obs_source_get_output_flags(src);

	if ((caps & OBS_SOURCE_VIDEO) == 0)
		return true;

	const char *name = obs_source_get_name(src);
	if (name && *name)
		obs_property_list_add_string(list, name, name);

	return true;
}

static obs_properties_t *shape_mask_properties(void *data)
{
	struct shape_mask_filter *f = data;

	obs_properties_t *props = obs_properties_create();

	obs_property_t *mode_list = obs_properties_add_list(props, SETTING_MASK_MODE,
							      obs_module_text("ShapeMask.MaskMode"),
							      OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(mode_list, obs_module_text("ShapeMask.MaskMode.Shape"), MASK_MODE_SHAPE);
	obs_property_list_add_int(mode_list, obs_module_text("ShapeMask.MaskMode.Source"), MASK_MODE_SOURCE);
	obs_property_set_modified_callback(mode_list, mask_mode_modified);

	obs_property_t *mask_source_list = obs_properties_add_list(props, SETTING_MASK_SOURCE,
								     obs_module_text("ShapeMask.MaskSource"),
								     OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(mask_source_list, obs_module_text("ShapeMask.MaskSource.None"), "");
	obs_enum_sources(enum_source_add, mask_source_list);

	obs_property_t *shape_list = obs_properties_add_list(props, SETTING_SHAPE, obs_module_text("ShapeMask.Shape"),
							       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);

	obs_property_list_add_int(shape_list, obs_module_text("ShapeMask.Shape.Circle"), SHAPE_CIRCLE);
	obs_property_list_add_int(shape_list, obs_module_text("ShapeMask.Shape.RoundedRect"), SHAPE_ROUNDED_RECT);
	obs_property_list_add_int(shape_list, obs_module_text("ShapeMask.Shape.Rectangle"), SHAPE_RECTANGLE);

	obs_property_set_modified_callback(shape_list, shape_modified);

	obs_properties_add_float_slider(props, SETTING_SCALE_X, obs_module_text("ShapeMask.ScaleX"), 0.1, 1.5, 0.01);
	obs_properties_add_float_slider(props, SETTING_SCALE_Y, obs_module_text("ShapeMask.ScaleY"), 0.1, 1.5, 0.01);

	obs_properties_add_float_slider(props, SETTING_OFFSET_X, obs_module_text("ShapeMask.OffsetX"), -0.5, 0.5,
					 0.005);
	obs_properties_add_float_slider(props, SETTING_OFFSET_Y, obs_module_text("ShapeMask.OffsetY"), -0.5, 0.5,
					 0.005);

	obs_properties_add_float_slider(props, SETTING_FEATHER, obs_module_text("ShapeMask.Feather"), 0.0, 0.20,
					 0.002);

	obs_properties_add_float_slider(props, SETTING_CORNER_RADIUS, obs_module_text("ShapeMask.CornerRadius"), 0.0,
					 0.5, 0.005);
	obs_properties_add_float_slider(props, SETTING_ROTATION, obs_module_text("ShapeMask.Rotation"), -180.0,
					 180.0, 1.0);

	obs_properties_add_bool(props, SETTING_MASK_INVERT, obs_module_text("ShapeMask.MaskInvert"));

	obs_property_t *border_enable =
		obs_properties_add_bool(props, SETTING_BORDER_ENABLE, obs_module_text("ShapeMask.BorderEnable"));

	obs_properties_add_color(props, SETTING_BORDER_COLOR, obs_module_text("ShapeMask.BorderColor"));

	obs_properties_add_float_slider(props, SETTING_BORDER_WIDTH, obs_module_text("ShapeMask.BorderWidth"), 0.0,
					 0.12, 0.001);
	obs_properties_add_float_slider(props, SETTING_BORDER_SOFT, obs_module_text("ShapeMask.BorderSoftness"), 0.0,
					 0.05, 0.001);
	obs_properties_add_float_slider(props, SETTING_BORDER_OPACITY, obs_module_text("ShapeMask.BorderOpacity"), 0.0,
					 1.0, 0.01);

	obs_properties_add_float_slider(props, SETTING_BORDER_GLOW, obs_module_text("ShapeMask.BorderGlow"), 0.0,
					 1.0, 0.01);

	obs_property_t *border_pulse =
		obs_properties_add_bool(props, SETTING_BORDER_PULSE, obs_module_text("ShapeMask.BorderPulse"));

	obs_properties_add_float_slider(props, SETTING_PULSE_SPEED, obs_module_text("ShapeMask.PulseSpeed"), 0.1,
					 12.0, 0.1);

	obs_properties_add_float_slider(props, SETTING_PULSE_AMOUNT, obs_module_text("ShapeMask.PulseAmount"), 0.0,
					 1.0, 0.05);

	obs_property_t *shadow_enable =
		obs_properties_add_bool(props, SETTING_SHADOW_ENABLE, obs_module_text("ShapeMask.ShadowEnable"));

	obs_properties_add_color(props, SETTING_SHADOW_COLOR, obs_module_text("ShapeMask.ShadowColor"));

	obs_properties_add_float_slider(props, SETTING_SHADOW_OFFSETX, obs_module_text("ShapeMask.ShadowOffsetX"),
					 -0.3, 0.3, 0.005);
	obs_properties_add_float_slider(props, SETTING_SHADOW_OFFSETY, obs_module_text("ShapeMask.ShadowOffsetY"),
					 -0.3, 0.3, 0.005);
	obs_properties_add_float_slider(props, SETTING_SHADOW_BLUR, obs_module_text("ShapeMask.ShadowBlur"), 0.001,
					 0.25, 0.002);
	obs_properties_add_float_slider(props, SETTING_SHADOW_OPACITY, obs_module_text("ShapeMask.ShadowOpacity"),
					 0.0, 1.0, 0.01);

	obs_property_t *animation = obs_properties_add_list(props, SETTING_ANIMATION,
							      obs_module_text("ShapeMask.Animation"),
							      OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(animation, obs_module_text("ShapeMask.Animation.None"), ANIM_NONE);
	obs_property_list_add_int(animation, obs_module_text("ShapeMask.Animation.Pulse"), ANIM_PULSE);
	obs_property_list_add_int(animation, obs_module_text("ShapeMask.Animation.Breathe"), ANIM_BREATHE);
	obs_property_list_add_int(animation, obs_module_text("ShapeMask.Animation.Rotate"), ANIM_ROTATE);
	obs_property_list_add_int(animation, obs_module_text("ShapeMask.Animation.Bounce"), ANIM_BOUNCE);
	obs_property_list_add_int(animation, obs_module_text("ShapeMask.Animation.Shake"), ANIM_SHAKE);
	obs_property_list_add_int(animation, obs_module_text("ShapeMask.Animation.Scale"), ANIM_SCALE);
	obs_properties_add_float_slider(props, SETTING_ANIM_SPEED, obs_module_text("ShapeMask.AnimationSpeed"), 0.0,
					 10.0, 0.1);
	obs_properties_add_float_slider(props, SETTING_ANIM_AMOUNT, obs_module_text("ShapeMask.AnimationAmount"), 0.0,
					 1.0, 0.01);

	obs_property_set_modified_callback(border_enable, border_enabled_modified);
	obs_property_set_modified_callback(border_pulse, border_pulse_modified);
	obs_property_set_modified_callback(shadow_enable, shadow_enabled_modified);

	if (f && f->source) {
		obs_data_t *settings = obs_source_get_settings(f->source);
		if (settings) {
			mask_mode_modified(props, NULL, settings);
			shape_modified(props, NULL, settings);
			border_enabled_modified(props, NULL, settings);
			border_pulse_modified(props, NULL, settings);
			shadow_enabled_modified(props, NULL, settings);
			obs_data_release(settings);
		}
	}

	return props;
}

static void shape_mask_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, SETTING_MASK_MODE, MASK_MODE_SHAPE);
	obs_data_set_default_string(settings, SETTING_MASK_SOURCE, "");

	obs_data_set_default_int(settings, SETTING_SHAPE, SHAPE_CIRCLE);

	obs_data_set_default_double(settings, SETTING_SCALE_X, 1.0);
	obs_data_set_default_double(settings, SETTING_SCALE_Y, 1.0);

	obs_data_set_default_double(settings, SETTING_OFFSET_X, 0.0);
	obs_data_set_default_double(settings, SETTING_OFFSET_Y, 0.0);

	obs_data_set_default_double(settings, SETTING_FEATHER, 0.01);

	obs_data_set_default_double(settings, SETTING_CORNER_RADIUS, 0.08);
	obs_data_set_default_double(settings, SETTING_ROTATION, 0.0);

	obs_data_set_default_bool(settings, SETTING_MASK_INVERT, false);

	obs_data_set_default_bool(settings, SETTING_BORDER_ENABLE, false);
	obs_data_set_default_int(settings, SETTING_BORDER_COLOR, 0xFFFFFFFF);
	obs_data_set_default_double(settings, SETTING_BORDER_WIDTH, 0.008);
	obs_data_set_default_double(settings, SETTING_BORDER_SOFT, 0.0015);
	obs_data_set_default_double(settings, SETTING_BORDER_OPACITY, 1.0);
	obs_data_set_default_double(settings, SETTING_BORDER_GLOW, 0.20);
	obs_data_set_default_bool(settings, SETTING_BORDER_PULSE, false);
	obs_data_set_default_double(settings, SETTING_PULSE_SPEED, 2.0);
	obs_data_set_default_double(settings, SETTING_PULSE_AMOUNT, 0.40);

	obs_data_set_default_bool(settings, SETTING_SHADOW_ENABLE, false);
	obs_data_set_default_int(settings, SETTING_SHADOW_COLOR, 0xB0000000);
	obs_data_set_default_double(settings, SETTING_SHADOW_OFFSETX, 0.015);
	obs_data_set_default_double(settings, SETTING_SHADOW_OFFSETY, 0.02);
	obs_data_set_default_double(settings, SETTING_SHADOW_BLUR, 0.03);
	obs_data_set_default_double(settings, SETTING_SHADOW_OPACITY, 0.6);

	obs_data_set_default_int(settings, SETTING_ANIMATION, ANIM_NONE);
	obs_data_set_default_double(settings, SETTING_ANIM_SPEED, 1.0);
	obs_data_set_default_double(settings, SETTING_ANIM_AMOUNT, 0.25);
}

struct obs_source_info shape_mask_filter_info = {
	.id = "obs_shape_window",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,

	.get_name = shape_mask_get_name,
	.create = shape_mask_create,
	.destroy = shape_mask_destroy,
	.update = shape_mask_update,

	.video_tick = shape_mask_video_tick,
	.video_render = shape_mask_video_render,

	.get_width = shape_mask_width,
	.get_height = shape_mask_height,

	.get_properties = shape_mask_properties,
	.get_defaults = shape_mask_defaults,
};

bool obs_module_load(void)
{
	obs_register_source(&shape_mask_filter_info);

	blog(LOG_INFO, "[obs-shape-window] plugin loaded (version 1.1.0)");

	return true;
}

void obs_module_unload(void)
{
}
