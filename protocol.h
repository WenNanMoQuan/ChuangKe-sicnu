/**
 * @file    protocol.h
 * @brief   端云交互数据协议 —— JSON 字段与结构体定义
 * @stage   阶段一：需求与系统方案设计（端云接口协议规范）
 *
 * 协议约定（详见《端云接口协议规范》）：
 *  设备 -> 云端  : { "device_id", "image_base64", "ultrasonic_cm", "battery", "ts" }
 *  云端 -> 设备  : {
 *      "blind_path": { "detected", "offset_direction", "offset_angle", "status" },
 *      "obstacles": [ { "type", "distance", "direction", "risk_level" } ],
 *      "feedback":  { "vibration", "buzzer", "led_color", "voice_text" },
 *      "degraded":  false
 *  }
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <Arduino.h>

/* ---------- 盲道状态枚举（阶段五状态机 5 态） ---------- */
enum BlindPathStatus {
    BP_FOLLOWING   = 0,   // 正常跟随
    BP_OFFSET_LEFT = 1,   // 左偏
    BP_OFFSET_RIGHT= 2,   // 右偏
    BP_TURNING     = 3,   // 转弯
    BP_END_DETECTED= 4    // 盲道终止
};

/* ---------- 障碍物风险等级（阶段五三级预警） ---------- */
enum RiskLevel {
    RISK_NONE = 0,
    RISK_LV1  = 1,        // 2~3 米 远距离
    RISK_LV2  = 2,        // 1~2 米 中距离
    RISK_LV3  = 3         // 0~1 米 近距离
};

/* ---------- 天气条件（离线闭环：运行时恒为 WX_UNKNOWN，湿滑档由物理按钮本地切换） ---------- */
enum WeatherCond {
    WX_CLEAR   = 0,   // 晴/多云（干燥，正常档）
    WX_RAIN    = 1,   // 雨（湿滑，更保守档）
    WX_SNOW    = 2,   // 雪/冰（湿滑，更保守档）
    WX_FOG     = 3,   // 雾/霾（视差/湿滑，更保守档）
    WX_UNKNOWN = 9    // 未知（默认按正常档，不主动切换）
};
// 湿滑类天气 -> 启用更保守预警档（阈值整体 +1m）
static inline bool weather_is_wet(WeatherCond w) {
    return w == WX_RAIN || w == WX_SNOW || w == WX_FOG;
}

/* ---------- 识别来源（离线闭环：端侧主 + 超声波安全底线） ---------- */
enum RunSource {
    SRC_LOCAL   = 0,   // 端侧 TFLite Micro 检测（唯一识别主路径，全程离线）
    SRC_CLOUD   = 1,   // 【保留未用】离线闭环架构下云端不参与引导，仅可选开发遥测
    SRC_ULTRA   = 2,   // 无模型/识别失败时的 HC-SR04 超声波安全底线（SRC_LOCAL 的兜底）
};

/* ---------- 云端返回的反馈指令 ---------- */
struct FeedbackCmd {
    uint8_t  vibration_pwm = 0;     // 0~100，对应 PWM 占空比
    bool     buzzer_on     = false; // 蜂鸣器开关
    uint32_t led_color     = 0x000000; // RGB 颜色
    String   voice_text    = "";    // 语音播报文本（送 MAX98357A TTS/预录）
};

/* ---------- 单条障碍物 ---------- */
struct Obstacle {
    String   type;        // vehicle / pedestrian / barrier ...
    float    distance;    // 米
    String   direction;   // left / center / right
    RiskLevel risk_level;
};

/* ---------- 完整识别结果（解析后供业务逻辑使用） ---------- */
struct RecognizeResult {
    bool      degraded     = false; // 云端降级标记
    bool      path_detected= false;
    BlindPathStatus path_status = BP_FOLLOWING;
    float     offset_angle = 0.0;   // 偏移角度（度）
    String    offset_dir   = "center";
    Obstacle  obstacles[4];         // 最多 4 个
    uint8_t   obstacle_cnt= 0;
    FeedbackCmd feedback;
    WeatherCond weather = WX_UNKNOWN; // 【离线闭环】天气不由云端下发，运行时恒为 WX_UNKNOWN；
                                       // 雨天/湿滑档由物理按钮本地切换（见 set_weather_mode）
    RunSource    source  = SRC_LOCAL; // 本次结果来源（端侧/云端/离线超声波）
    float        det_conf= 0.0f;      // 端侧最高置信度（用于“低置信兜底”判定）
    // 端侧检测扩展（混合架构新增字段）
    bool        zebra_detected  = false; // 斑马线
    bool        manhole_detected= false; // 井盖
    float       blindpath_conf  = 0.0f;  // 盲道检测置信度
};

/* ---------- JSON KEY 常量（避免拼写错误） ---------- */
#define K_DEVICE_ID      "device_id"
#define K_IMAGE          "image_base64"
#define K_ULTRA          "ultrasonic_cm"
#define K_BATTERY        "battery"
#define K_TS             "ts"
#define K_BLIND_PATH     "blind_path"
#define K_DETECTED       "detected"
#define K_OFFSET_DIR     "offset_direction"
#define K_OFFSET_ANGLE   "offset_angle"
#define K_STATUS         "status"
#define K_OBSTACLES      "obstacles"
#define K_TYPE           "type"
#define K_DISTANCE       "distance"
#define K_DIRECTION      "direction"
#define K_RISK           "risk_level"
#define K_FEEDBACK       "feedback"
#define K_VIBRATION      "vibration"
#define K_BUZZER         "buzzer"
#define K_LED_COLOR      "led_color"
#define K_VOICE          "voice_text"
#define K_DEGRADED       "degraded"
#define K_WEATHER        "weather"          // 仅可选开发遥测字段: "clear"/"rain"/"snow"/"fog"

#endif /* PROTOCOL_H */
