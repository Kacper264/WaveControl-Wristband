#include "include.h"

static const char* TAG = "AI";

// =======================================================
// CONFIG MODEL
// =======================================================
// En Float32 : 1 élément = 4 octets
#define EXPECTED_INPUT_BYTES  (INPUT_SIZE * sizeof(float))
#define EXPECTED_OUTPUT_BYTES (OUTPUT_SIZE * sizeof(float))

// Taille de l'arena (ajuster si AllocateTensors échoue)
constexpr int kTensorArenaSize = 120 * 1024;

namespace {
    const tflite::Model* model = nullptr;
    tflite::MicroInterpreter* interpreter = nullptr;
    TfLiteTensor* input_tensor = nullptr;
    TfLiteTensor* output_tensor = nullptr;

    // Allocation statique de l'arena
    static uint8_t tensor_arena[kTensorArenaSize];
}

// =======================================================
// INIT MODEL
// =======================================================
bool init_model() {
    ESP_LOGI(TAG, "Initialisation du modèle Float32...");

    model = tflite::GetModel(model_float32_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGE(TAG, "Erreur: Version du schéma TFLite incompatible !");
        return false;
    }

    // 1. Augmentez le nombre d'opérations autorisées (ex: 20 au lieu de 15)
    // 2. Ajoutez explicitement l'opération manquante
    static tflite::MicroMutableOpResolver<20> resolver;
    
    // Ajoutez ExpandDims ici :
    if (resolver.AddExpandDims() != kTfLiteOk) {
        ESP_LOGE(TAG, "Échec de l'ajout de ExpandDims au resolver");
        return false;
    }

    // Ajoutez les autres opérations courantes (selon l'architecture de votre modèle)
    resolver.AddSoftmax();
    resolver.AddConv2D();
    resolver.AddFullyConnected();
    resolver.AddReshape();
    resolver.AddQuantize();   // Parfois nécessaire même en float32
    resolver.AddDequantize(); // Parfois nécessaire même en float32

    // Création de l'interpréteur
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize);

    interpreter = &static_interpreter;

    // Tentative d'allocation
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        // Si l'erreur persiste ici après l'ajout de l'Op, 
        // c'est que kTensorArenaSize est vraiment trop petit.
        ESP_LOGE(TAG, "AllocateTensors() a échoué ! (Code: %d)", allocate_status);
        return false;
    }

    input_tensor  = interpreter->input(0);
    output_tensor = interpreter->output(0);

    ESP_LOGI(TAG, "Modèle prêt. Arena utilisée : %d octets", interpreter->arena_used_bytes());
    return true;
}

// =======================================================
// RUN INFERENCE
// =======================================================
bool run_inference(float* input_data, float* output_data) {
    if (!interpreter) return false;

    // 1. Copie des données d'entrée (Directe car Float32 -> Float32)
    // On accède au pointeur float du tenseur via data.f
    memcpy(input_tensor->data.f, input_data, EXPECTED_INPUT_BYTES);

    // 2. Lancement de l'inférence
    if (interpreter->Invoke() != kTfLiteOk) {
        ESP_LOGE(TAG, "Erreur lors de l'exécution (Invoke)");
        return false;
    }

    // 3. Récupération des résultats
    memcpy(output_data, output_tensor->data.f, EXPECTED_OUTPUT_BYTES);

    return true;
}