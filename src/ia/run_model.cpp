#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "esp_log.h"
#include "esp_heap_caps.h"   // 👈 PSRAM allocator

#include "ia/run_model.h"
#include "ia/model_data.h"

/* -------------------------------------------------------------------------- */
/* Config                                                                      */
/* -------------------------------------------------------------------------- */

#define INPUT_SIZE   900
#define OUTPUT_SIZE  6

static const char* TAG = "AI";

/* Tu peux réduire plus tard si besoin */
constexpr int kTensorArenaSize = 60 * 1024;

/* -------------------------------------------------------------------------- */
/* TFLite globals                                                              */
/* -------------------------------------------------------------------------- */

namespace {

const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;

TfLiteTensor* input_tensor  = nullptr;
TfLiteTensor* output_tensor = nullptr;

/* Arena en PSRAM (au lieu de DRAM) */
uint8_t* tensor_arena = nullptr;

}

/* -------------------------------------------------------------------------- */
/* Init model                                                                  */
/* -------------------------------------------------------------------------- */
bool init_model()
{
    ESP_LOGI(TAG, "Init TensorFlow model");
    ESP_LOGI(TAG, "Free internal 8-bit: %u",
         (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT));
ESP_LOGI(TAG, "Largest internal block: %u",
         (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
ESP_LOGI(TAG, "Requested arena: %u", (unsigned)kTensorArenaSize);

    model = tflite::GetModel(model_quant_int8_tflite);

    /* Allocation en RAM interne (pas de PSRAM) */
    tensor_arena = (uint8_t*) heap_caps_malloc(
        kTensorArenaSize,
        MALLOC_CAP_8BIT
    );

    if (!tensor_arena) {
        ESP_LOGE(TAG, "Internal RAM allocation failed");
        return false;
    }

    ESP_LOGI(TAG, "Tensor arena allocated in internal RAM: %d bytes", kTensorArenaSize);

    /* Register only needed ops */
    static tflite::MicroMutableOpResolver<8> resolver;

    resolver.AddFullyConnected();
    resolver.AddSoftmax();
    resolver.AddReshape();
    resolver.AddQuantize();
    resolver.AddDequantize();
    resolver.AddExpandDims();
    resolver.AddConv2D();
    resolver.AddMaxPool2D();

    static tflite::MicroInterpreter static_interpreter(
        model,
        resolver,
        tensor_arena,
        kTensorArenaSize
    );

    interpreter = &static_interpreter;

    if (interpreter->AllocateTensors() != kTfLiteOk) {
        ESP_LOGE(TAG, "AllocateTensors FAILED");
        return false;
    }

    input_tensor  = interpreter->input(0);
    output_tensor = interpreter->output(0);

    ESP_LOGI(TAG, "Input bytes: %d", input_tensor->bytes);
    ESP_LOGI(TAG, "Output bytes: %d", output_tensor->bytes);

    if (input_tensor->bytes != INPUT_SIZE) {
        ESP_LOGE(TAG, "Input size mismatch");
        return false;
    }

    if (output_tensor->bytes != OUTPUT_SIZE) {
        ESP_LOGE(TAG, "Output size mismatch");
        return false;
    }

    ESP_LOGI(TAG, "Model ready (INT8 quantized)");

    return true;
}

/* -------------------------------------------------------------------------- */
/* Inference                                                                   */
/* -------------------------------------------------------------------------- */

bool run_inference(float* input_data, float* output_data)
{
    if (!interpreter) return false;

    /* -------- Quantize input -------- */

    const float in_scale = input_tensor->params.scale;
    const int   in_zero  = input_tensor->params.zero_point;

    for (int i = 0; i < INPUT_SIZE; i++)
    {
        int8_t q = (int8_t)(input_data[i] / in_scale + in_zero);
        input_tensor->data.int8[i] = q;
    }

    /* -------- Run inference -------- */

    if (interpreter->Invoke() != kTfLiteOk)
    {
        ESP_LOGE(TAG, "Invoke FAILED");
        return false;
    }

    /* -------- Dequantize output -------- */

    const float out_scale = output_tensor->params.scale;
    const int   out_zero  = output_tensor->params.zero_point;

    for (int i = 0; i < OUTPUT_SIZE; i++)
    {
        int8_t q = output_tensor->data.int8[i];
        output_data[i] = (q - out_zero) * out_scale;
    }

    return true;
}
