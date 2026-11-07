#include "face_tracker.h"

#include <windows.h>
#include <onnxruntime_c_api.h>
#include <obs-module.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FACE_CONFIDENCE_THRESHOLD 0.65f

struct face_tracker {
	CRITICAL_SECTION lock;
	CONDITION_VARIABLE condition;
	HANDLE thread;
	bool stopping;
	bool enabled;
	bool has_pending_frame;
	uint8_t pending_frame[FACE_TRACKER_BGR_SIZE];
	char *model_path;

	float result_x;
	float result_y;
	bool result_found;
	uint64_t result_timestamp;
	uint64_t result_sequence;
};

struct face_detector {
	HMODULE runtime_module;
	const OrtApi *api;
	OrtEnv *env;
	OrtSession *session;
	OrtMemoryInfo *memory_info;
	OrtAllocator *allocator;
	char *input_name;
	char *output_names[12];
};

typedef const OrtApiBase *(ORT_API_CALL *ort_get_api_base_fn)(void);

static size_t face_detector_output_index(const struct face_detector *detector, const char *name);

static void face_detector_release(struct face_detector *detector)
{
	if (detector->api) {
		for (size_t i = 0; i < 12; i++) {
			if (detector->output_names[i])
				detector->allocator->Free(detector->allocator, detector->output_names[i]);
		}

		if (detector->input_name)
			detector->allocator->Free(detector->allocator, detector->input_name);
		if (detector->memory_info)
			detector->api->ReleaseMemoryInfo(detector->memory_info);
		if (detector->session)
			detector->api->ReleaseSession(detector->session);
		if (detector->env)
			detector->api->ReleaseEnv(detector->env);
	}

	if (detector->runtime_module)
		FreeLibrary(detector->runtime_module);
	memset(detector, 0, sizeof(*detector));
}

static bool face_detector_status(struct face_detector *detector, OrtStatus *status, const char *operation)
{
	if (!status)
		return true;

	blog(LOG_ERROR, "[obs-shape-mask] Face tracker %s failed: %s", operation,
	     detector->api->GetErrorMessage(status));
	detector->api->ReleaseStatus(status);
	return false;
}

static bool face_detector_open(struct face_detector *detector, const char *model_path)
{
	wchar_t model_path_wide[MAX_PATH];
	wchar_t runtime_path[MAX_PATH];
	HMODULE module = NULL;
	DWORD path_length;
	int model_path_length;

	memset(detector, 0, sizeof(*detector));

	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				(LPCWSTR)(uintptr_t)&face_detector_open, &module)) {
		blog(LOG_ERROR, "[obs-shape-mask] Face tracker could not locate its plugin directory.");
		return false;
	}

	path_length = GetModuleFileNameW(module, runtime_path, MAX_PATH);
	if (!path_length || path_length >= MAX_PATH) {
		blog(LOG_ERROR, "[obs-shape-mask] Face tracker could not resolve its runtime path.");
		return false;
	}

	while (path_length && runtime_path[path_length - 1] != L'\\')
		path_length--;
	if (path_length + 16 >= MAX_PATH) {
		blog(LOG_ERROR, "[obs-shape-mask] Face tracker runtime path is too long.");
		return false;
	}
	wcscpy_s(runtime_path + path_length, MAX_PATH - path_length, L"onnxruntime.dll");

	detector->runtime_module = LoadLibraryW(runtime_path);
	if (!detector->runtime_module) {
		blog(LOG_ERROR, "[obs-shape-mask] Face tracker could not load onnxruntime.dll.");
		return false;
	}

	ort_get_api_base_fn get_api_base =
		(ort_get_api_base_fn)(uintptr_t)GetProcAddress(detector->runtime_module, "OrtGetApiBase");
	if (!get_api_base) {
		blog(LOG_ERROR, "[obs-shape-mask] Face tracker runtime does not export OrtGetApiBase.");
		face_detector_release(detector);
		return false;
	}

	const OrtApiBase *api_base = get_api_base();
	detector->api = api_base ? api_base->GetApi(ORT_API_VERSION) : NULL;
	if (!detector->api) {
		blog(LOG_ERROR, "[obs-shape-mask] Face tracker runtime API version is incompatible.");
		face_detector_release(detector);
		return false;
	}

	model_path_length =
		MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, model_path, -1, model_path_wide, MAX_PATH);
	if (!model_path_length) {
		blog(LOG_ERROR, "[obs-shape-mask] Face tracker model path is invalid.");
		face_detector_release(detector);
		return false;
	}

	OrtSessionOptions *session_options = NULL;
	if (!face_detector_status(detector,
				  detector->api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "obs-shape-mask", &detector->env),
				  "environment initialization") ||
	    !face_detector_status(detector, detector->api->CreateSessionOptions(&session_options), "session setup") ||
	    !face_detector_status(detector, detector->api->SetIntraOpNumThreads(session_options, 1),
				  "thread configuration") ||
	    !face_detector_status(detector, detector->api->SetInterOpNumThreads(session_options, 1),
				  "thread configuration") ||
	    !face_detector_status(detector,
				  detector->api->CreateSession(detector->env, model_path_wide, session_options,
							       &detector->session),
				  "model loading")) {
		if (session_options)
			detector->api->ReleaseSessionOptions(session_options);
		face_detector_release(detector);
		return false;
	}
	detector->api->ReleaseSessionOptions(session_options);

	if (!face_detector_status(detector, detector->api->GetAllocatorWithDefaultOptions(&detector->allocator),
				  "allocator setup") ||
	    !face_detector_status(detector,
				  detector->api->SessionGetInputName(detector->session, 0, detector->allocator,
								     &detector->input_name),
				  "model input discovery") ||
	    !face_detector_status(detector,
				  detector->api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault,
								     &detector->memory_info),
				  "memory setup")) {
		face_detector_release(detector);
		return false;
	}

	size_t output_count = 0;
	if (!face_detector_status(detector, detector->api->SessionGetOutputCount(detector->session, &output_count),
				  "model output discovery") ||
	    output_count != 12) {
		blog(LOG_ERROR, "[obs-shape-mask] Face tracker model has an unsupported output layout.");
		face_detector_release(detector);
		return false;
	}

	for (size_t i = 0; i < 12; i++) {
		if (!face_detector_status(detector,
					  detector->api->SessionGetOutputName(detector->session, i, detector->allocator,
									      &detector->output_names[i]),
					  "model output discovery")) {
			face_detector_release(detector);
			return false;
		}
	}

	for (int stride = 8; stride <= 32; stride *= 2) {
		char output_name[16];
		const char *prefixes[] = {"cls", "obj", "bbox", "kps"};
		for (size_t i = 0; i < sizeof(prefixes) / sizeof(prefixes[0]); i++) {
			snprintf(output_name, sizeof(output_name), "%s_%d", prefixes[i], stride);
			if (face_detector_output_index(detector, output_name) == 12) {
				blog(LOG_ERROR, "[obs-shape-mask] Face tracker model is missing output %s.",
				     output_name);
				face_detector_release(detector);
				return false;
			}
		}
	}

	return true;
}

static size_t face_detector_output_index(const struct face_detector *detector, const char *name)
{
	for (size_t i = 0; i < 12; i++) {
		if (detector->output_names[i] && strcmp(detector->output_names[i], name) == 0)
			return i;
	}
	return 12;
}

static bool face_detector_validate_output(struct face_detector *detector, OrtValue *output, size_t expected_count,
					  const char *name)
{
	OrtTensorTypeAndShapeInfo *shape_info = NULL;
	if (!face_detector_status(detector, detector->api->GetTensorTypeAndShape(output, &shape_info),
				  "output shape discovery"))
		return false;

	ONNXTensorElementDataType element_type = ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
	size_t element_count = 0;
	bool valid = face_detector_status(detector, detector->api->GetTensorElementType(shape_info, &element_type),
					  "output type discovery") &&
		     face_detector_status(detector,
					  detector->api->GetTensorShapeElementCount(shape_info, &element_count),
					  "output size discovery");
	detector->api->ReleaseTensorTypeAndShapeInfo(shape_info);

	if (!valid)
		return false;
	if (element_type != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT || element_count != expected_count) {
		blog(LOG_ERROR,
		     "[obs-shape-mask] Face tracker model output %s has an unsupported tensor shape or type.", name);
		return false;
	}

	return true;
}

static bool face_detector_run(struct face_detector *detector, const uint8_t *bgr_image, float previous_x,
			      float previous_y, bool have_previous, float *face_x, float *face_y)
{
	const int64_t input_shape[] = {1, 3, FACE_TRACKER_HEIGHT, FACE_TRACKER_WIDTH};
	const size_t plane_size = FACE_TRACKER_WIDTH * FACE_TRACKER_HEIGHT;
	float *input_data = malloc(plane_size * 3 * sizeof(float));
	OrtValue *input = NULL;
	OrtValue *outputs[12] = {0};
	bool found = false;
	float best_rank = -1.0f;

	if (!input_data)
		return false;

	for (size_t i = 0; i < plane_size; i++) {
		input_data[i] = (float)bgr_image[i * 3];
		input_data[plane_size + i] = (float)bgr_image[i * 3 + 1];
		input_data[plane_size * 2 + i] = (float)bgr_image[i * 3 + 2];
	}

	if (!face_detector_status(detector,
				  detector->api->CreateTensorWithDataAsOrtValue(
					  detector->memory_info, input_data, plane_size * 3 * sizeof(float),
					  input_shape, 4, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &input),
				  "input setup")) {
		free(input_data);
		return false;
	}

	OrtStatus *status = detector->api->Run(detector->session, NULL, (const char *const *)&detector->input_name,
					       (const OrtValue *const *)&input, 1,
					       (const char *const *)detector->output_names, 12, outputs);
	if (!face_detector_status(detector, status, "inference")) {
		detector->api->ReleaseValue(input);
		free(input_data);
		return false;
	}

	for (int stride = 8; stride <= 32; stride *= 2) {
		char cls_name[16];
		char obj_name[16];
		char bbox_name[16];
		snprintf(cls_name, sizeof(cls_name), "cls_%d", stride);
		snprintf(obj_name, sizeof(obj_name), "obj_%d", stride);
		snprintf(bbox_name, sizeof(bbox_name), "bbox_%d", stride);

		size_t cls_index = face_detector_output_index(detector, cls_name);
		size_t obj_index = face_detector_output_index(detector, obj_name);
		size_t bbox_index = face_detector_output_index(detector, bbox_name);
		if (cls_index == 12 || obj_index == 12 || bbox_index == 12)
			continue;

		float *cls = NULL;
		float *obj = NULL;
		float *bbox = NULL;
		size_t cell_count = (FACE_TRACKER_WIDTH / (size_t)stride) * (FACE_TRACKER_HEIGHT / (size_t)stride);
		if (!face_detector_validate_output(detector, outputs[cls_index], cell_count, cls_name) ||
		    !face_detector_validate_output(detector, outputs[obj_index], cell_count, obj_name) ||
		    !face_detector_validate_output(detector, outputs[bbox_index], cell_count * 4, bbox_name))
			goto cleanup;

		if (!face_detector_status(detector,
					  detector->api->GetTensorMutableData(outputs[cls_index], (void **)&cls),
					  "classification output") ||
		    !face_detector_status(detector,
					  detector->api->GetTensorMutableData(outputs[obj_index], (void **)&obj),
					  "objectness output") ||
		    !face_detector_status(detector,
					  detector->api->GetTensorMutableData(outputs[bbox_index], (void **)&bbox),
					  "box output"))
			goto cleanup;

		for (size_t cell = 0; cell < cell_count; cell++) {
			float score = sqrtf(fmaxf(0.0f, fminf(1.0f, cls[cell])) * fmaxf(0.0f, fminf(1.0f, obj[cell])));
			if (score < FACE_CONFIDENCE_THRESHOLD || !isfinite(score))
				continue;

			size_t columns = FACE_TRACKER_WIDTH / (size_t)stride;
			float center_x =
				((float)(cell % columns) + bbox[cell * 4]) * (float)stride / (float)FACE_TRACKER_WIDTH;
			float center_y = ((float)(cell / columns) + bbox[cell * 4 + 1]) * (float)stride /
					 (float)FACE_TRACKER_HEIGHT;
			float distance = have_previous ? sqrtf((center_x - previous_x) * (center_x - previous_x) +
							       (center_y - previous_y) * (center_y - previous_y))
						       : 0.0f;
			float rank = score - (have_previous ? distance * 0.35f : 0.0f);
			if (rank > best_rank) {
				best_rank = rank;
				*face_x = center_x;
				*face_y = center_y;
				found = true;
			}
		}
	}

cleanup:
	for (size_t i = 0; i < 12; i++) {
		if (outputs[i])
			detector->api->ReleaseValue(outputs[i]);
	}
	detector->api->ReleaseValue(input);
	free(input_data);
	return found;
}

static DWORD WINAPI face_tracker_thread(void *data)
{
	struct face_tracker *tracker = data;
	struct face_detector detector = {0};
	bool detector_ready = false;
	bool initialization_attempted = false;
	uint8_t frame[FACE_TRACKER_BGR_SIZE];

	for (;;) {
		EnterCriticalSection(&tracker->lock);
		while (!tracker->stopping && (!tracker->enabled || !tracker->has_pending_frame))
			SleepConditionVariableCS(&tracker->condition, &tracker->lock, INFINITE);
		if (tracker->stopping) {
			LeaveCriticalSection(&tracker->lock);
			break;
		}

		memcpy(frame, tracker->pending_frame, sizeof(frame));
		tracker->has_pending_frame = false;
		float previous_x = tracker->result_x;
		float previous_y = tracker->result_y;
		bool have_previous = tracker->result_found;
		LeaveCriticalSection(&tracker->lock);

		if (!initialization_attempted) {
			initialization_attempted = true;
			detector_ready = face_detector_open(&detector, tracker->model_path);
		}
		if (!detector_ready)
			continue;

		float x = 0.5f;
		float y = 0.5f;
		bool found = face_detector_run(&detector, frame, previous_x, previous_y, have_previous, &x, &y);

		EnterCriticalSection(&tracker->lock);
		tracker->result_found = found;
		if (found) {
			tracker->result_x = x;
			tracker->result_y = y;
		}
		tracker->result_timestamp = GetTickCount64();
		tracker->result_sequence++;
		LeaveCriticalSection(&tracker->lock);
	}

	face_detector_release(&detector);
	return 0;
}

struct face_tracker *face_tracker_create(const char *model_path)
{
	struct face_tracker *tracker = calloc(1, sizeof(*tracker));
	if (!tracker)
		return NULL;

	size_t path_length = strlen(model_path) + 1;
	tracker->model_path = malloc(path_length);
	if (!tracker->model_path) {
		free(tracker);
		return NULL;
	}
	memcpy(tracker->model_path, model_path, path_length);
	InitializeCriticalSection(&tracker->lock);
	InitializeConditionVariable(&tracker->condition);
	tracker->thread = CreateThread(NULL, 0, face_tracker_thread, tracker, 0, NULL);
	if (!tracker->thread) {
		DeleteCriticalSection(&tracker->lock);
		free(tracker->model_path);
		free(tracker);
		return NULL;
	}

	return tracker;
}

void face_tracker_destroy(struct face_tracker *tracker)
{
	if (!tracker)
		return;

	EnterCriticalSection(&tracker->lock);
	tracker->stopping = true;
	WakeConditionVariable(&tracker->condition);
	LeaveCriticalSection(&tracker->lock);

	WaitForSingleObject(tracker->thread, INFINITE);
	CloseHandle(tracker->thread);
	DeleteCriticalSection(&tracker->lock);
	free(tracker->model_path);
	free(tracker);
}

void face_tracker_set_enabled(struct face_tracker *tracker, bool enabled)
{
	if (!tracker)
		return;

	EnterCriticalSection(&tracker->lock);
	if (tracker->enabled != enabled) {
		tracker->enabled = enabled;
		tracker->has_pending_frame = false;
		tracker->result_found = false;
	}
	WakeConditionVariable(&tracker->condition);
	LeaveCriticalSection(&tracker->lock);
}

void face_tracker_submit(struct face_tracker *tracker, const uint8_t *bgr_image)
{
	if (!tracker || !bgr_image)
		return;

	EnterCriticalSection(&tracker->lock);
	if (tracker->enabled && !tracker->stopping) {
		memcpy(tracker->pending_frame, bgr_image, FACE_TRACKER_BGR_SIZE);
		tracker->has_pending_frame = true;
		WakeConditionVariable(&tracker->condition);
	}
	LeaveCriticalSection(&tracker->lock);
}

bool face_tracker_get_result(struct face_tracker *tracker, float *x, float *y, bool *found, uint64_t *timestamp,
			     uint64_t *sequence)
{
	if (!tracker)
		return false;

	EnterCriticalSection(&tracker->lock);
	*x = tracker->result_x;
	*y = tracker->result_y;
	*found = tracker->result_found;
	*timestamp = tracker->result_timestamp;
	*sequence = tracker->result_sequence;
	LeaveCriticalSection(&tracker->lock);
	return true;
}
