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

// Exact standardization parameters extracted from your latest Colab Stage 2.1
const float mean_values[4] = { 30.54642188f, 69.84349824f, 0.02481317f, -0.08868648f };
const float scale_values[4] = { 3.88920078f, 6.80413667f, 0.45390116f, 1.80804241f };

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

// Helper: replace argmax with a logic to validate predictions based on confidence thresholds
static uint8_t get_validated_label(float* output_data, int size, float* max_val) {
    float prob_bg = output_data[0];
    float prob_nuisance = output_data[1];
    float prob_fire = output_data[2];

    *max_val = prob_bg;
    uint8_t predicted_class = 0;

    if (prob_fire > 0.5f) {
        predicted_class = 2;
        *max_val = prob_fire;
    }
    else if (prob_nuisance > 0.8f) {
        predicted_class = 1;
        *max_val = prob_nuisance;
    }
    return predicted_class;
}

void tiny_ml_task(void *pvParameters) {
    setupTinyML();

    float prev_temp = -1.0f; 
    float prev_hum  = -1.0f;

    while (1) {
        SensorData sd = {0.0f, 0.0f};
        
        if (xQueuePeek(xQueueSensorData, &sd, 0) == pdTRUE) {
            
            if (sd.temp != prev_temp || sd.hum != prev_hum) {
                
                if (prev_temp == -1.0f) {
                    prev_temp = sd.temp;
                    prev_hum = sd.hum;
                }

                // Standard feature extraction layer
                float temp_rate = sd.temp - prev_temp;
                float humi_rate_cali = (sd.hum - prev_hum) * 2.0f;

                prev_temp = sd.temp;
                prev_hum = sd.hum;

                // Preprocessing mapping block matching Colab StandardScaler topology
                input->data.f[0] = (sd.temp - mean_values[0]) / scale_values[0];
                input->data.f[1] = (sd.hum - mean_values[1]) / scale_values[1];
                input->data.f[2] = (temp_rate - mean_values[2]) / scale_values[2];
                input->data.f[3] = (humi_rate_cali - mean_values[3]) / scale_values[3];

                unsigned long start_us = micros();
                TfLiteStatus invoke_status = interpreter->Invoke();
                unsigned long duration_us = micros() - start_us;
                float duration_ms = duration_us / 1000.0f; 

                if (invoke_status == kTfLiteOk) {
                    float confidence_score = 0.0f;
                    uint8_t predicted_class = get_validated_label(output->data.f, 3, &confidence_score);
                    
                    float prob_bg       = output->data.f[0];
                    float prob_nuisance = output->data.f[1]; 
                    float prob_fire     = output->data.f[2]; 
                    uint32_t arena_used = interpreter->arena_used_bytes();

                    tinyml_update_all(predicted_class, confidence_score, sd.temp, sd.hum);

                    char time_str[30];
                    struct tm timeinfo;
                    if (!getLocalTime(&timeinfo)) {
                        strcpy(time_str, "Time-Not-Sync");
                    } else {
                        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo);
                    }

                    // Log format: [TINYML_LOG],timestamp,temp,humi,temp_rate,humi_rate_cali,predicted_class,confidence_score,prob_bg,prob_nuisance,prob_fire,inference_time_ms,arena_used
                    Serial.printf("[TINYML_LOG],%s,%.2f,%.2f,%.4f,%.4f,%d,%.4f,%.4f,%.4f,%.4f,%.2f,%u\n",
                                time_str, sd.temp, sd.hum, temp_rate, humi_rate_cali / 2.0f, 
                                predicted_class, confidence_score, 
                                prob_bg, prob_nuisance, prob_fire, duration_ms, arena_used);
                } else {
                    error_reporter->Report("Inference failed");
                }
            }

        }
        vTaskDelay(pdMS_TO_TICKS(500)); 
    }
}