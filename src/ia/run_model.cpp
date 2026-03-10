#include "include.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"

static const char* TAG = "AI";

// =======================================================
// CONFIG MODEL
// =======================================================
#define EXPECTED_INPUT_BYTES  (INPUT_SIZE * sizeof(float))
#define EXPECTED_OUTPUT_BYTES (OUTPUT_SIZE * sizeof(float))

constexpr size_t kTensorArenaSize = 120 * 1024;

namespace {
    const tflite::Model* model = nullptr;
    tflite::MicroInterpreter* interpreter = nullptr;
    TfLiteTensor* input_tensor = nullptr;
    TfLiteTensor* output_tensor = nullptr;

    // Arena allouée dynamiquement en PSRAM
    uint8_t* tensor_arena = nullptr;
}

// =======================================================
// INIT MODEL
// =======================================================
bool init_model() {
    ESP_LOGE(TAG, "Initialisation du modèle Float32...");

    // Charger le modèle
    model = tflite::GetModel(model_float32_tflite);
    if (!model) {
        ESP_LOGE(TAG, "Impossible de charger le modèle TFLite");
        return false;
    }

    if (model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(TAG, "Version du schéma TFLite incompatible !");
        return false;
    }

    printf("DRAM libre avant alloc : %d", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    printf("PSRAM libre avant alloc: %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    // Allouer l'arena une seule fois
    if (tensor_arena == nullptr) {
        tensor_arena = (uint8_t*)heap_caps_malloc(
            kTensorArenaSize,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
        );

        if (!tensor_arena) {
            ESP_LOGE(TAG, "Allocation PSRAM échouée pour %u bytes", (unsigned)kTensorArenaSize);
            return false;
        }

        ESP_LOGE(TAG, "Arena PSRAM allouée : %u bytes", (unsigned)kTensorArenaSize);
    }

    ESP_LOGE(TAG, "DRAM libre après alloc : %d", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGE(TAG, "PSRAM libre après alloc: %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    // Resolver
    static tflite::MicroMutableOpResolver<20> resolver;
    static bool resolver_initialized = false;

    if (!resolver_initialized) {
        if (resolver.AddExpandDims() != kTfLiteOk) {
            ESP_LOGE(TAG, "Échec de l'ajout de ExpandDims");
            return false;
        }
        if (resolver.AddSoftmax() != kTfLiteOk) {
            ESP_LOGE(TAG, "Échec de l'ajout de Softmax");
            return false;
        }
        if (resolver.AddConv2D() != kTfLiteOk) {
            ESP_LOGE(TAG, "Échec de l'ajout de Conv2D");
            return false;
        }
        if (resolver.AddFullyConnected() != kTfLiteOk) {
            ESP_LOGE(TAG, "Échec de l'ajout de FullyConnected");
            return false;
        }
        if (resolver.AddReshape() != kTfLiteOk) {
            ESP_LOGE(TAG, "Échec de l'ajout de Reshape");
            return false;
        }
        if (resolver.AddQuantize() != kTfLiteOk) {
            ESP_LOGE(TAG, "Échec de l'ajout de Quantize");
            return false;
        }
        if (resolver.AddDequantize() != kTfLiteOk) {
            ESP_LOGE(TAG, "Échec de l'ajout de Dequantize");
            return false;
        }

        resolver_initialized = true;
    }

    // Créer l'interpréteur une seule fois
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize
    );
    interpreter = &static_interpreter;

    // Allocation des tenseurs
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        ESP_LOGE(TAG, "AllocateTensors() a échoué ! (Code: %d)", allocate_status);
        return false;
    }

    input_tensor = interpreter->input(0);
    output_tensor = interpreter->output(0);

    if (!input_tensor || !output_tensor) {
        ESP_LOGE(TAG, "Impossible de récupérer les tenseurs d'entrée/sortie");
        return false;
    }

    ESP_LOGI(TAG, "Modèle prêt. Arena utilisée : %d octets", interpreter->arena_used_bytes());
    return true;
}

// =======================================================
// RUN INFERENCE
// =======================================================
bool run_inference(float* input_data, float* output_data) {
    if (!interpreter || !input_tensor || !output_tensor) {
        ESP_LOGE(TAG, "Interpréteur ou tenseurs non initialisés");
        return false;
    }

    memcpy(input_tensor->data.f, input_data, EXPECTED_INPUT_BYTES);

    if (interpreter->Invoke() != kTfLiteOk) {
        ESP_LOGE(TAG, "Erreur lors de l'exécution (Invoke)");
        return false;
    }

    memcpy(output_data, output_tensor->data.f, EXPECTED_OUTPUT_BYTES);
    return true;
}