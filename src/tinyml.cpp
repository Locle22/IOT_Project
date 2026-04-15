#include "tinyml.h"

namespace {
    tflite::ErrorReporter* error_reporter = nullptr;
    const tflite::Model* model = nullptr;
    tflite::MicroInterpreter* interpreter = nullptr;
    TfLiteTensor* input = nullptr;
    TfLiteTensor* output = nullptr;
    
    constexpr int kTensorArenaSize = 16 * 1024;
    uint8_t tensor_arena[kTensorArenaSize];

    // Support variables for precision timing
    unsigned long base_millis = 0;
    bool is_first_sample = true;
    float last_temp = -1.0;
    float last_hum = -1.0;
}

void setupTinyML() {
    static tflite::MicroErrorReporter micro_error_reporter;
    error_reporter = &micro_error_reporter;

    model = tflite::GetModel(dht_anomaly_model_tflite);
    static tflite::AllOpsResolver resolver;
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
    interpreter = &static_interpreter;

    interpreter->AllocateTensors();
    input = interpreter->input(0);
    output = interpreter->output(0);
    Serial.println("TinyML Engine initialized with Precision Timing.");
}

void TaskTinyML(void *pvParameters) {
    setupTinyML();
    SensorData receivedData;

    while (1) {
        if (xQueueReceive(sensorQueue, &receivedData, portMAX_DELAY) == pdPASS) {
            
            unsigned long now = millis();

            if (is_first_sample) {
                base_millis = now;
                is_first_sample = false;
            }

            unsigned long relative_time = now - base_millis;

            // Measure inference time
            unsigned long start_micros = micros();

            input->data.f[0] = receivedData.temperature;
            input->data.f[1] = receivedData.humidity;

            // Run inference
            TfLiteStatus invoke_status = interpreter->Invoke();
            
            unsigned long end_micros = micros();
            unsigned long inference_us = end_micros - start_micros;

            if (invoke_status == kTfLiteOk) {
                float result = output->data.f[0];
                
                float dT = (last_temp < 0) ? 0 : (receivedData.temperature - last_temp);
                float dH = (last_hum < 0) ? 0 : (receivedData.humidity - last_hum);

                Serial.printf("[Time:%lu ms] ", relative_time);
                Serial.printf("[Duration:%lu us] ", inference_us);
                Serial.printf("Result: %.4f | T:%.1f (dT:%.1f), H:%.1f (dH:%.1f)\n", 
                              result, receivedData.temperature, dT, receivedData.humidity, dH);

                last_temp = receivedData.temperature;
                last_hum = receivedData.humidity;
            } else {
                Serial.println("Error: Inference failed!");
            }
        }
    }
}