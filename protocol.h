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

#endif /* PROTOCOL_H */
