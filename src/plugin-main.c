#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/math-defs.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-shape-window", "en-US")

#define SETTING_SHAPE         "shape_type"
#define SETTING_SCALE         "shape_scale"
#define SETTING_OFFSET_X      "offset_x"
#define SETTING_OFFSET_Y      "offset_y"
#define SETTING_FEATHER       "feather"
#define SETTING_CORNER_RADIUS "corner_radius"

#define SETTING_BORDER_ENABLE "border_enabled"
#define SETTING_BORDER_COLOR  "border_color"
#define SETTING_BORDER_WIDTH  "border_width"
#define SETTING_BORDER_GLOW   "border_glow"
#define SETTING_BORDER_PULSE  "border_pulse"
#define SETTING_PULSE_SPEED   "pulse_speed"
#define SETTING_PULSE_AMOUNT  "pulse_amount"

enum shape_type {
    SHAPE_CIRCLE = 0,
    SHAPE_ROUNDED_RECT = 1,
    SHAPE_ELLIPSE = 2,
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

    gs_eparam_t *param_border_enabled;
    gs_eparam_t *param_border_color;
    gs_eparam_t *param_border_width;
    gs_eparam_t *param_border_glow;
    gs_eparam_t *param_border_pulse;
    gs_eparam_t *param_pulse_speed;
    gs_eparam_t *param_pulse_amount;
    gs_eparam_t *param_time;

    int shape;
    float scale;
    float offset_x;
    float offset_y;
    float feather;
    float corner_radius;

    bool border_enabled;
    uint32_t border_color;
    float border_width;
    float border_glow;
    bool border_pulse;
    float pulse_speed;
    float pulse_amount;

    float elapsed_time;
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
    f->scale = (float)obs_data_get_double(settings, SETTING_SCALE);
    f->offset_x = (float)obs_data_get_double(settings, SETTING_OFFSET_X);
    f->offset_y = (float)obs_data_get_double(settings, SETTING_OFFSET_Y);
    f->feather = (float)obs_data_get_double(settings, SETTING_FEATHER);
    f->corner_radius =
        (float)obs_data_get_double(settings, SETTING_CORNER_RADIUS);

    f->border_enabled =
        obs_data_get_bool(settings, SETTING_BORDER_ENABLE);
    f->border_color =
        (uint32_t)obs_data_get_int(settings, SETTING_BORDER_COLOR);
    f->border_width =
        (float)obs_data_get_double(settings, SETTING_BORDER_WIDTH);
    f->border_glow =
        (float)obs_data_get_double(settings, SETTING_BORDER_GLOW);
    f->border_pulse =
        obs_data_get_bool(settings, SETTING_BORDER_PULSE);
    f->pulse_speed =
        (float)obs_data_get_double(settings, SETTING_PULSE_SPEED);
    f->pulse_amount =
        (float)obs_data_get_double(settings, SETTING_PULSE_AMOUNT);

    // Keep values sane if a scene or old saved profile contains
    // out-of-range data.
    f->scale = f->scale < 0.05f ? 0.05f : f->scale;
    f->scale = f->scale > 1.5f ? 1.5f : f->scale;

    f->feather = f->feather < 0.0f ? 0.0f : f->feather;
    f->feather = f->feather > 0.20f ? 0.20f : f->feather;

    f->corner_radius =
        f->corner_radius < 0.0f ? 0.0f : f->corner_radius;
    f->corner_radius =
        f->corner_radius > 0.50f ? 0.50f : f->corner_radius;

    f->border_width =
        f->border_width < 0.0f ? 0.0f : f->border_width;
    f->border_width =
        f->border_width > 0.08f ? 0.08f : f->border_width;

    f->border_glow =
        f->border_glow < 0.0f ? 0.0f : f->border_glow;
    f->border_glow =
        f->border_glow > 1.0f ? 1.0f : f->border_glow;

    f->pulse_speed =
        f->pulse_speed < 0.0f ? 0.0f : f->pulse_speed;
    f->pulse_speed =
        f->pulse_speed > 12.0f ? 12.0f : f->pulse_speed;

    f->pulse_amount =
        f->pulse_amount < 0.0f ? 0.0f : f->pulse_amount;
    f->pulse_amount =
        f->pulse_amount > 1.0f ? 1.0f : f->pulse_amount;
}

static void *shape_mask_create(obs_data_t *settings, obs_source_t *source)
{
    struct shape_mask_filter *f =
        bzalloc(sizeof(struct shape_mask_filter));

    f->source = source;

    char *effect_path = obs_module_file("shape_mask.effect");

    obs_enter_graphics();
    f->effect = gs_effect_create_from_file(effect_path, NULL);
    obs_leave_graphics();

    bfree(effect_path);

    if (!f->effect) {
        blog(LOG_ERROR,
             "[obs-shape-window] Failed to load shape_mask.effect");
        bfree(f);
        return NULL;
    }

    f->param_shape_type =
        gs_effect_get_param_by_name(f->effect, "shape_type");
    f->param_scale =
        gs_effect_get_param_by_name(f->effect, "shape_scale");
    f->param_offset =
        gs_effect_get_param_by_name(f->effect, "offset");
    f->param_uv_size =
        gs_effect_get_param_by_name(f->effect, "uv_size");
    f->param_feather =
        gs_effect_get_param_by_name(f->effect, "feather");
    f->param_corner_radius =
        gs_effect_get_param_by_name(f->effect, "corner_radius");

    f->param_border_enabled =
        gs_effect_get_param_by_name(f->effect, "border_enabled");
    f->param_border_color =
        gs_effect_get_param_by_name(f->effect, "border_color");
    f->param_border_width =
        gs_effect_get_param_by_name(f->effect, "border_width");
    f->param_border_glow =
        gs_effect_get_param_by_name(f->effect, "border_glow");
    f->param_border_pulse =
        gs_effect_get_param_by_name(f->effect, "border_pulse");
    f->param_pulse_speed =
        gs_effect_get_param_by_name(f->effect, "pulse_speed");
    f->param_pulse_amount =
        gs_effect_get_param_by_name(f->effect, "pulse_amount");
    f->param_time =
        gs_effect_get_param_by_name(f->effect, "time_seconds");

    shape_mask_update(f, settings);
    return f;
}

static void shape_mask_destroy(void *data)
{
    struct shape_mask_filter *f = data;

    if (f->effect) {
        obs_enter_graphics();
        gs_effect_destroy(f->effect);
        obs_leave_graphics();
    }

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

    if (!f->border_pulse)
        return;

    f->elapsed_time += seconds;

    if (f->elapsed_time > 10000.0f)
        f->elapsed_time -= 10000.0f;
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

    if (!obs_source_process_filter_begin(
            f->source, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING))
        return;

    struct vec2 uv_size;
    vec2_set(&uv_size, (float)width, (float)height);

    struct vec2 offset;
    vec2_set(&offset, f->offset_x, f->offset_y);

    struct vec4 border_color;
    vec4_from_rgba(&border_color, f->border_color);

    gs_effect_set_int(f->param_shape_type, f->shape);
    gs_effect_set_float(f->param_scale, f->scale);
    gs_effect_set_vec2(f->param_offset, &offset);
    gs_effect_set_vec2(f->param_uv_size, &uv_size);
    gs_effect_set_float(f->param_feather, f->feather);
    gs_effect_set_float(f->param_corner_radius, f->corner_radius);

    gs_effect_set_bool(f->param_border_enabled, f->border_enabled);
    gs_effect_set_vec4(f->param_border_color, &border_color);
    gs_effect_set_float(f->param_border_width, f->border_width);
    gs_effect_set_float(f->param_border_glow, f->border_glow);
    gs_effect_set_bool(f->param_border_pulse, f->border_pulse);
    gs_effect_set_float(f->param_pulse_speed, f->pulse_speed);
    gs_effect_set_float(f->param_pulse_amount, f->pulse_amount);
    gs_effect_set_float(f->param_time, f->elapsed_time);

    obs_source_process_filter_end(
        f->source, f->effect, width, height);
}

static bool border_enabled_modified(
    obs_properties_t *props,
    obs_property_t *prop,
    obs_data_t *settings)
{
    UNUSED_PARAMETER(prop);

    bool enabled =
        obs_data_get_bool(settings, SETTING_BORDER_ENABLE);

    obs_property_set_visible(
        obs_properties_get(props, SETTING_BORDER_COLOR), enabled);
    obs_property_set_visible(
        obs_properties_get(props, SETTING_BORDER_WIDTH), enabled);
    obs_property_set_visible(
        obs_properties_get(props, SETTING_BORDER_GLOW), enabled);
    obs_property_set_visible(
        obs_properties_get(props, SETTING_BORDER_PULSE), enabled);

    bool pulse =
        obs_data_get_bool(settings, SETTING_BORDER_PULSE);

    obs_property_set_visible(
        obs_properties_get(props, SETTING_PULSE_SPEED),
        enabled && pulse);
    obs_property_set_visible(
        obs_properties_get(props, SETTING_PULSE_AMOUNT),
        enabled && pulse);

    return true;
}

static bool border_pulse_modified(
    obs_properties_t *props,
    obs_property_t *prop,
    obs_data_t *settings)
{
    UNUSED_PARAMETER(prop);

    bool visible =
        obs_data_get_bool(settings, SETTING_BORDER_ENABLE) &&
        obs_data_get_bool(settings, SETTING_BORDER_PULSE);

    obs_property_set_visible(
        obs_properties_get(props, SETTING_PULSE_SPEED), visible);
    obs_property_set_visible(
        obs_properties_get(props, SETTING_PULSE_AMOUNT), visible);

    return true;
}

static bool shape_modified(
    obs_properties_t *props,
    obs_property_t *prop,
    obs_data_t *settings)
{
    UNUSED_PARAMETER(prop);

    int shape = (int)obs_data_get_int(settings, SETTING_SHAPE);

    obs_property_set_visible(
        obs_properties_get(props, SETTING_CORNER_RADIUS),
        shape == SHAPE_ROUNDED_RECT);

    return true;
}

static obs_properties_t *shape_mask_properties(void *data)
{
    struct shape_mask_filter *f = data;

    obs_properties_t *props = obs_properties_create();

    obs_property_t *shape_list =
        obs_properties_add_list(
            props,
            SETTING_SHAPE,
            obs_module_text("ShapeMask.Shape"),
            OBS_COMBO_TYPE_LIST,
            OBS_COMBO_FORMAT_INT);

    obs_property_list_add_int(
        shape_list,
        obs_module_text("ShapeMask.Shape.Circle"),
        SHAPE_CIRCLE);
    obs_property_list_add_int(
        shape_list,
        obs_module_text("ShapeMask.Shape.RoundedRect"),
        SHAPE_ROUNDED_RECT);
    obs_property_list_add_int(
        shape_list,
        obs_module_text("ShapeMask.Shape.Ellipse"),
        SHAPE_ELLIPSE);

    obs_property_set_modified_callback(
        shape_list, shape_modified);

    obs_properties_add_float_slider(
        props,
        SETTING_SCALE,
        obs_module_text("ShapeMask.Scale"),
        0.1, 1.5, 0.01);

    obs_properties_add_float_slider(
        props,
        SETTING_OFFSET_X,
        obs_module_text("ShapeMask.OffsetX"),
        -0.5, 0.5, 0.005);

    obs_properties_add_float_slider(
        props,
        SETTING_OFFSET_Y,
        obs_module_text("ShapeMask.OffsetY"),
        -0.5, 0.5, 0.005);

    obs_properties_add_float_slider(
        props,
        SETTING_FEATHER,
        obs_module_text("ShapeMask.Feather"),
        0.0, 0.20, 0.002);

    obs_properties_add_float_slider(
        props,
        SETTING_CORNER_RADIUS,
        obs_module_text("ShapeMask.CornerRadius"),
        0.0, 0.5, 0.005);

    obs_property_t *border_enable =
        obs_properties_add_bool(
            props,
            SETTING_BORDER_ENABLE,
            obs_module_text("ShapeMask.BorderEnable"));

    obs_properties_add_color(
        props,
        SETTING_BORDER_COLOR,
        obs_module_text("ShapeMask.BorderColor"));

    obs_properties_add_float_slider(
        props,
        SETTING_BORDER_WIDTH,
        obs_module_text("ShapeMask.BorderWidth"),
        0.0, 0.08, 0.001);

    obs_properties_add_float_slider(
        props,
        SETTING_BORDER_GLOW,
        obs_module_text("ShapeMask.BorderGlow"),
        0.0, 1.0, 0.01);

    obs_property_t *border_pulse =
        obs_properties_add_bool(
            props,
            SETTING_BORDER_PULSE,
            obs_module_text("ShapeMask.BorderPulse"));

    obs_properties_add_float_slider(
        props,
        SETTING_PULSE_SPEED,
        obs_module_text("ShapeMask.PulseSpeed"),
        0.1, 12.0, 0.1);

    obs_properties_add_float_slider(
        props,
        SETTING_PULSE_AMOUNT,
        obs_module_text("ShapeMask.PulseAmount"),
        0.0, 1.0, 0.05);

    obs_property_set_modified_callback(
        border_enable, border_enabled_modified);
    obs_property_set_modified_callback(
        border_pulse, border_pulse_modified);

    /* Apply correct initial visibility as soon as the panel opens, instead
       of waiting for the user to touch a checkbox or the shape dropdown
       first. */
    if (f && f->source) {
        obs_data_t *settings = obs_source_get_settings(f->source);
        if (settings) {
            shape_modified(props, NULL, settings);
            border_enabled_modified(props, NULL, settings);
            border_pulse_modified(props, NULL, settings);
            obs_data_release(settings);
        }
    }

    return props;
}

static void shape_mask_defaults(obs_data_t *settings)
{
    obs_data_set_default_int(
        settings, SETTING_SHAPE, SHAPE_CIRCLE);

    obs_data_set_default_double(
        settings, SETTING_SCALE, 1.0);

    obs_data_set_default_double(
        settings, SETTING_OFFSET_X, 0.0);
    obs_data_set_default_double(
        settings, SETTING_OFFSET_Y, 0.0);

    obs_data_set_default_double(
        settings, SETTING_FEATHER, 0.01);

    obs_data_set_default_double(
        settings, SETTING_CORNER_RADIUS, 0.08);

    obs_data_set_default_bool(
        settings, SETTING_BORDER_ENABLE, false);

    obs_data_set_default_int(
        settings, SETTING_BORDER_COLOR, 0xFFFFFFFF);

    obs_data_set_default_double(
        settings, SETTING_BORDER_WIDTH, 0.008);

    obs_data_set_default_double(
        settings, SETTING_BORDER_GLOW, 0.20);

    obs_data_set_default_bool(
        settings, SETTING_BORDER_PULSE, false);

    obs_data_set_default_double(
        settings, SETTING_PULSE_SPEED, 2.0);

    obs_data_set_default_double(
        settings, SETTING_PULSE_AMOUNT, 0.40);
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

    blog(LOG_INFO,
         "[obs-shape-window] plugin loaded (version 0.3.0)");

    return true;
}

void obs_module_unload(void)
{
}
