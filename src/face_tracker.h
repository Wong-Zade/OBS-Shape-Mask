#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FACE_TRACKER_WIDTH 320
#define FACE_TRACKER_HEIGHT 320
#define FACE_TRACKER_BGR_SIZE (FACE_TRACKER_WIDTH * FACE_TRACKER_HEIGHT * 3)

struct face_tracker;

struct face_tracker *face_tracker_create(const char *model_path);
void face_tracker_destroy(struct face_tracker *tracker);
void face_tracker_set_enabled(struct face_tracker *tracker, bool enabled);
void face_tracker_submit(struct face_tracker *tracker, const uint8_t *bgr_image);
bool face_tracker_get_result(struct face_tracker *tracker, float *x, float *y, bool *found, uint64_t *timestamp,
			     uint64_t *sequence);
