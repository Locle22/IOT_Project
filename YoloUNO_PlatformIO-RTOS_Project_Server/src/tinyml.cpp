#include "tinyml.h"

// Globals for TinyML interpreter
namespace
{
    tflite::ErrorReporter *error_reporter = nullptr;
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *output = nullptr;
    constexpr int kTensorArenaSize = 16 * 1024;
    uint8_t tensor_arena[kTensorArenaSize];
} // namespace

void setupTinyML()
{
    static tflite::MicroErrorReporter micro_error_reporter;
    error_reporter = &micro_error_reporter;

    model = tflite::GetModel(dht_model_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        error_reporter->Report("Model version mismatch: %d vs %d",
                               model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }

    static tflite::AllOpsResolver resolver;
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
    interpreter = &static_interpreter;

    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk)
    {
        error_reporter->Report("AllocateTensors() failed");
        return;
    }

    input = interpreter->input(0);
    output = interpreter->output(0);
}

// Helper function to find argmax (class with highest probability)
static uint8_t argmax(float* output_data, int size) {
    uint8_t max_index = 0;
    float max_value = output_data[0];
    for (int i = 1; i < size; i++) {
        if (output_data[i] > max_value) {
            max_value = output_data[i];
            max_index = i;
        }
    }
    return max_index;
}

void tiny_ml_task(void *pvParameters)
{
    setupTinyML();

    while (1)
    {
        // Đọc dữ liệu cảm biến mới nhất từ Queue
        SensorData sd = {0.0f, 0.0f};
        xQueuePeek(xQueueSensorData, &sd, 0);  // Non-blocking peek

        // Measure inference time
        unsigned long start_us = micros();

        input->data.f[0] = sd.temp;
        input->data.f[1] = sd.hum;

        // Run inference
        TfLiteStatus invoke_status = interpreter->Invoke();

        unsigned long duration_us = micros() - start_us;

        if (invoke_status != kTfLiteOk)
        {
            error_reporter->Report("Invoke failed");
            return;
        }

        // Get arena used bytes after inference
        uint32_t arena_used = interpreter->arena_used_bytes();

        // Get output - 3 classes: Background(0), Fire(1), Nuisance(2)
        uint8_t predicted_class = argmax(output->data.f, 3);

        // Update global metrics
        tinyml_update_metrics(predicted_class, duration_us, arena_used);

        // Send result to queue for other tasks (non-blocking)
        TinyMLResult mlResult = {
            .predicted_class = predicted_class,
            .timestamp = millis(),
            .duration_us = duration_us,
            .arena_used_bytes = arena_used,
            .temp = sd.temp,
            .hum = sd.hum
        };
        xQueueOverwrite(xQueueTinyMLResult, &mlResult);

        vTaskDelay(2000);
    }
}