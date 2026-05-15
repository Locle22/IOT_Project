#include "tinyml.h"

// Globals cho TinyML interpreter
namespace {
    tflite::ErrorReporter *error_reporter = nullptr;
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *output = nullptr;
    constexpr int kTensorArenaSize = 16 * 1024; 
    uint8_t tensor_arena[kTensorArenaSize];
}

void setupTinyML()
{
    Serial.println("TensorFlow Lite Init....");
    static tflite::MicroErrorReporter micro_error_reporter;
    error_reporter = &micro_error_reporter;

    model = tflite::GetModel(dht_model_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION)
    {
        error_reporter->Report("Model provided is schema version %d, not equal to supported version %d.",
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

    Serial.printf("TensorFlow Lite Micro initialized. Input dimensions: %d\n", input->dims->data[1]);
}

// Helper: find the label with the highest probability (argmax) and also return the confidence score
static uint8_t argmax(float* output_data, int size, float* max_val) {
    uint8_t max_index = 0;
    *max_val = output_data[0];
    for (int i = 1; i < size; i++) {
        if (output_data[i] > *max_val) {
            *max_val = output_data[i];
            max_index = i;
        }
    }
    return max_index;
}

// Task TinyML
void tiny_ml_task(void *pvParameters) {
    setupTinyML();

    float prev_temp = -1.0f; 
    float prev_hum  = -1.0f;
    unsigned long prev_time_ms = 0;

    while (1) {
        SensorData sd = {0.0f, 0.0f};
        
        if (xQueuePeek(xQueueSensorData, &sd, 0) == pdTRUE) {
            
            unsigned long current_time_ms = millis();
            float temp_rate_per_sec = 0.0f;
            float humi_rate_per_sec  = 0.0f;

            // Only calculate rates if we have valid previous readings and time
            if (prev_temp != -1.0f && prev_hum != -1.0f && prev_time_ms != 0) {
                float delta_t_sec = (current_time_ms - prev_time_ms) / 1000.0f;
                if (delta_t_sec > 0.0f) {
                    temp_rate_per_sec = (sd.temp - prev_temp) / delta_t_sec;
                    humi_rate_per_sec  = (sd.hum - prev_hum) / delta_t_sec;
                }
            }

            // Always update previous values for the next iteration
            prev_temp = sd.temp;
            prev_hum  = sd.hum;
            prev_time_ms = current_time_ms;

            input->data.f[0] = sd.temp;
            input->data.f[1] = sd.hum;
            input->data.f[2] = temp_rate_per_sec;
            input->data.f[3] = humi_rate_per_sec;

            // Run inference
            unsigned long start_us = micros();
            TfLiteStatus invoke_status = interpreter->Invoke();
            unsigned long duration_us = micros() - start_us;
            float duration_ms = duration_us / 1000.0f; 

            if (invoke_status == kTfLiteOk) {
                float confidence_score = 0.0f;
                uint8_t predicted_class = argmax(output->data.f, 3, &confidence_score);
                
                float prob_bg       = output->data.f[0];
                float prob_fire     = output->data.f[1];
                float prob_nuisance = output->data.f[2];
                uint32_t arena_used = interpreter->arena_used_bytes();

                // Update metrics and log entry
                tinyml_update_all(predicted_class, confidence_score, sd.temp, sd.hum);

                // Format print serial and log
                Serial.printf("[TINYML_LOG],%.2f,%.2f,%.4f,%.4f,%d,%.4f,%.4f,%.4f,%.4f,%.2f,%u\n",
                              sd.temp, sd.hum, temp_rate_per_sec, humi_rate_per_sec, 
                              predicted_class, confidence_score, 
                              prob_bg, prob_nuisance, prob_fire, duration_ms, arena_used);
            } else {
                error_reporter->Report("Inference failed");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}