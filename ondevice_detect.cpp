/**
 * @file    ondevice_detect.cpp
 * @brief   端侧 AI 识别实现（TFLite Micro 封装 + 后处理解码）
 * @stage   阶段八（端侧 YOLO-nano 检测）
 *
 * 本文件链接 tensorFlowLite（PlatformIO lib_deps）。纯主循环/云端/离线场景若未引用本
 * 文件，则不会链接 TFLM，仍可在无 TFLM 的 ArduinoIDE 环境编译（见 ondevice_detect.h 说明）。
 *
 * ⚠️ 模型输出约定（训练/转换脚本须严格遵守，见 model_tools/README）：
 *    输出张量形状 [1, N, 6]，N 为候选框数（建议 100~200）。每框 6 个 float/uint8：
 *      [0] x_center 归一化 0~1（框中心横向相对图宽）
 *      [1] y_center 归一化 0~1
 *      [2] width     归一化 0~1（框宽相对图宽）
 *      [3] height    归一化 0~1
 *      [4] class_id  0~4（盲道/斑马线/井盖/障碍/其他），四舍五入取整
 *      [5] confidence 0~1
 *    支持 uint8 量化输出（按张量 scale/zero_point 反量化）与 float 输出两种。
 */
#include "ondevice_detect.h"
#include <SD.h>
#include "core_logic.h"   // risk_of()（天气模式距离→风险）
#include <TensorFlowLite.h>
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/version.h"

/* ---------------- 全局状态 ---------------- */
static bool              g_ready = false;
static uint8_t          *g_model_buf = nullptr;   // 从 SD 读入的模型字节（常驻）
static const tflite::Model *g_model = nullptr;
static tflite::MicroInterpreter *g_interp = nullptr;
static TfLiteTensor     *g_in = nullptr;
static TfLiteTensor     *g_out = nullptr;
static uint8_t          *g_arena = nullptr;

/* ---------------- 工具：RGB565 → 输入张量（uint8 RGB HWC） ---------------- */
static inline uint8_t clip8(int v) { return v < 0 ? 0 : (v > 255 ? 255 : (uint8_t)v); }

static void rgb565_to_input(const uint8_t *src, size_t sw, size_t sh,
                            uint8_t *tensor, int dw, int dh) {
    // 最近邻下采样：模型输入为 uint8 RGB HWC（量化模型由 input 张量 scale/zero 处理）
    for (int y = 0; y < dh; y++) {
        int sy = (int)((y * sh) / (float)dh);
        if (sy >= (int)sh) sy = (int)sh - 1;
        for (int x = 0; x < dw; x++) {
            int sx = (int)((x * sw) / (float)dw);
            if (sx >= (int)sw) sx = (int)sw - 1;
            const uint8_t *p = src + ((sy * sw + sx) << 1);
            uint16_t px = (p[1] << 8) | p[0];
            uint8_t r = clip8(((px >> 11) & 0x1F) * 255 / 31);
            uint8_t g = clip8(((px >> 5)  & 0x3F) * 255 / 63);
            uint8_t b = clip8(( px        & 0x1F) * 255 / 31);
            uint8_t *dst = tensor + (y * dw + x) * 3;
            dst[0] = r; dst[1] = g; dst[2] = b;
        }
    }
}

/* ---------------- 工具：反量化输出（uint8量化 → float） ---------------- */
static float dequant(const TfLiteTensor *t, int i) {
    if (t->type == kTfLiteUInt8) {
        return ((float)t->data.uint8[i] - t->params.zero_point) * t->params.scale;
    }
    return t->data.f[i];   // float 直接返回
}

/* ---------------- 初始化：从 SD 加载模型 ---------------- */
bool local_detect_init() {
    g_ready = false;
    if (!SD.exists(MODEL_PATH_SD)) {
        Serial.printf("[AI] 模型不存在 %s，端侧检测不可用（走云端/离线兜底）\n", MODEL_PATH_SD);
        return false;
    }
    File f = SD.open(MODEL_PATH_SD, FILE_READ);
    if (!f) { Serial.println("[AI] 打开模型失败"); return false; }
    size_t sz = f.size();
    if (sz == 0 || sz > MODEL_MAX_BYTES) { Serial.printf("[AI] 模型尺寸异常 %u\n", sz); f.close(); return false; }
    g_model_buf = (uint8_t*)malloc(sz);
    if (!g_model_buf) { Serial.println("[AI] 模型内存分配失败"); f.close(); return false; }
    if (f.read(g_model_buf, sz) != (int)sz) { Serial.println("[AI] 模型读入失败"); free(g_model_buf); g_model_buf = nullptr; f.close(); return false; }
    f.close();

    g_model = tflite::GetModel(g_model_buf);
    if (g_model->version() != TFLITE_SCHEMA_VERSION) {
        Serial.println("[AI] 模型 schema 版本不匹配"); free(g_model_buf); g_model_buf = nullptr; return false;
    }
    if (g_arena == nullptr) g_arena = (uint8_t*)malloc(TFLM_TENSOR_ARENA);
    if (!g_arena) { Serial.println("[AI] tensor arena 分配失败"); free(g_model_buf); g_model_buf = nullptr; return false; }

    // 操作解析器：覆盖 YOLO-nano / NanoDet 常见算子子集（如报 “op not found”，在此追加 Register）
    static tflite::MicroMutableOpResolver<24> resolver;
    resolver.AddConv2D();
    resolver.AddDepthwiseConv2D();
    resolver.AddFullyConnected();
    resolver.AddRelu();
    resolver.AddRelu6();
    resolver.AddLeakyRelu();
    resolver.AddPrelu();
    resolver.AddAdd();
    resolver.AddMul();
    resolver.AddMean();
    resolver.AddMaxPool2D();
    resolver.AddAveragePool2D();
    resolver.AddSoftmax();
    resolver.AddLogistic();
    resolver.AddSigmoid();
    resolver.AddTanh();
    resolver.AddPad();
    resolver.AddResizeBilinear();
    resolver.AddConcatenation();
    resolver.AddTranspose();
    resolver.AddReshape();
    resolver.AddQuantize();
    resolver.AddDequantize();
    resolver.AddStridedSlice();
    resolver.AddSlice();
    resolver.AddCast();

    static tflite::MicroInterpreter static_interp(g_model, resolver, g_arena, TFLM_TENSOR_ARENA);
    g_interp = &static_interp;
    if (g_interp->AllocateTensors() != kTfLiteOk) {
        Serial.println("[AI] 张量分配失败（arena 不足？调大 TFLM_TENSOR_ARENA）"); return false;
    }
    g_in  = g_interp->input(0);
    g_out = g_interp->output(0);
    g_ready = true;
    Serial.printf("[AI] 模型加载成功 in:[%dx%dx%d] out:[%dx%dx%d]\n",
                   g_in->dims->data[1], g_in->dims->data[2], g_in->dims->data[3],
                   g_out->dims->data[1], g_out->dims->data[2], g_out->dims->data[3]);
    return true;
}

bool local_detect_ready() { return g_ready; }

void local_detect_print_status() {
    Serial.printf("[AI] ready=%d model=%s\n", g_ready, MODEL_PATH_SD);
}

/* ---------------- 后处理：NMS + 业务语义映射 ---------------- */
struct DetBox { float x, y, w, h, conf; int cls; };

static float iou(const DetBox &a, const DetBox &b) {
    float x1 = max(a.x - a.w/2, b.x - b.w/2), x2 = min(a.x + a.w/2, b.x + b.w/2);
    float y1 = max(a.y - a.h/2, b.y - b.h/2), y2 = min(a.y + a.h/2, b.y + b.h/2);
    float iw = max(0.0f, x2 - x1), ih = max(0.0f, y2 - y1);
    float inter = iw * ih;
    float areaA = a.w * a.h, areaB = b.w * b.h;
    float uni = areaA + areaB - inter;
    return uni > 0 ? inter / uni : 0;
}

bool local_detect_run(const uint8_t *rgb, size_t src_w, size_t src_h,
                      RecognizeResult &out) {
    if (!g_ready || !g_in || !g_out) return false;
    memset(&out, 0, sizeof(out));   // 清零（保留默认枚举初值）
    out.source = SRC_LOCAL;

    // 1) 预处理 RGB565 → 输入张量
    uint8_t *in_buf = g_in->data.uint8;
    rgb565_to_input(rgb, src_w, src_h, in_buf, LOCAL_DETECT_W, LOCAL_DETECT_H);

    // 2) 推理
    if (g_interp->Invoke() != kTfLiteOk) { Serial.println("[AI] 推理失败"); return false; }

    // 3) 解码输出 [1, N, 6]
    int N = g_out->dims->data[1];
    const int STEP = 6;
    DetBox dets[16]; int ndet = 0;          // 单帧最多保留 16 框（够用）
    float max_conf = 0.0f;

    for (int i = 0; i < N && ndet < 16; i++) {
        int base = i * STEP;
        float conf = dequant(g_out, base + 5);
        if (conf < DET_CONF_THRESH) continue;
        DetBox d;
        d.x = dequant(g_out, base + 0);
        d.y = dequant(g_out, base + 1);
        d.w = dequant(g_out, base + 2);
        d.h = dequant(g_out, base + 3);
        d.cls = (int)(dequant(g_out, base + 4) + 0.5f);
        d.conf = conf;
        if (d.cls < 0 || d.cls >= CLASS_COUNT) continue;
        // NMS：同类重叠取高置信
        bool keep = true;
        for (int j = 0; j < ndet; j++) {
            if (dets[j].cls == d.cls && iou(dets[j], d) > DET_NMS_THRESH) {
                if (d.conf > dets[j].conf) dets[j] = d;  // 替换为更高置信
                keep = false; break;
            }
        }
        if (keep) dets[ndet++] = d;
        if (conf > max_conf) max_conf = conf;
    }
    out.det_conf = max_conf;

    // 4) 业务语义映射
    float best_bp = 0.0f; DetBox bp_box; bool has_bp = false;
    for (int i = 0; i < ndet; i++) {
        const DetBox &d = dets[i];
        switch (d.cls) {
            case CLASS_BLINDPATH:
                out.path_detected = true;
                out.blindpath_conf = d.conf;
                if (d.conf > best_bp) { best_bp = d.conf; bp_box = d; has_bp = true; }
                break;
            case CLASS_ZEBRA:
                out.zebra_detected = true;
                break;
            case CLASS_MANHOLE:
                out.manhole_detected = true;
                // 井盖视为地面隐患：按障碍 LV1 提示
                if (out.obstacle_cnt < 4) {
                    Obstacle &o = out.obstacles[out.obstacle_cnt++];
                    o.type = "manhole"; o.direction = (d.x < 0.4f ? "left" : (d.x > 0.6f ? "right" : "center"));
                    o.distance = OBSTACLE_FOCAL_PX * 0.3f / (d.h * src_h); // 井盖直径约0.3m
                    o.risk_level = RISK_LV1;
                }
                break;
            case CLASS_OBSTACLE: {
                if (out.obstacle_cnt < 4) {
                    Obstacle &o = out.obstacles[out.obstacle_cnt++];
                    o.type = "obstacle"; o.direction = (d.x < 0.4f ? "left" : (d.x > 0.6f ? "right" : "center"));
                    o.distance = OBSTACLE_FOCAL_PX * OBSTACLE_REAL_H_M / (d.h * src_h);
                    o.risk_level = (RiskLevel)risk_of(o.distance);   // 自动套用天气模式
                }
                break;
            }
            default: break; // CLASS_OTHER 忽略
        }
    }

    // 盲道状态：用最高置信盲道框的横向位置判定偏移
    if (has_bp) {
        if      (bp_box.x < 0.40f) out.path_status = BP_OFFSET_LEFT;
        else if (bp_box.x > 0.60f) out.path_status = BP_OFFSET_RIGHT;
        else                       out.path_status = BP_FOLLOWING;
    } else {
        out.path_status = BP_FOLLOWING;   // 未检测到盲道默认跟随
    }
    return true;
}
