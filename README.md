# 04 外设驱动模块

> 对应规划：阶段二（外设连接）/ 阶段三（全外设驱动开发）

## 一、功能（本模块做什么）
统一封装 5 类交互外设的驱动接口：
| 外设 | 接口 | 关键函数 |
|---|---|---|
| DRV2605L 震动马达 | I2C(38/39) | `vibration_set(pwm)` 0~100 强度、`vibration_stop()` |
| WS2812B 灯带(8灯) | 单线(41) | `led_set_color()`、`led_blink()`、`led_off()` |
| 有源蜂鸣器 | GPIO(40) | `buzzer_on/off/beep()` |
| HC-SR04 超声波 | Trig42/Echo2 | `ultrasonic_read_cm()` |
| MAX98357A I2S 功放 | I2S(4/5/6) | `audio_play_pcm()`、`audio_say(text)` |

## 二、作用
- 这是端侧“执行反馈”的**硬件执行层**。06 业务逻辑算出的 `FeedbackCmd` 最终都落到这里变成真实震动/声音/灯光。
- 离线兜底（06）也依赖 `ultrasonic_read_cm()` 与 `buzzer/vibration`。

## 三、拼接方法
1. `#include "peripherals.h"`，依赖 `Wire`、`Adafruit_NeoPixel`、`driver/i2s`。
2. 在 `main.cpp` 启动早期调用 `peripherals_init()`（必须在 `camera_init` 之前或之后均可，但需先 `Wire.begin`）。
3. 业务逻辑调用示例：
   ```cpp
   vibration_set(result.feedback.vibration_pwm);
   led_set_color(result.feedback.led_color);
   if (result.feedback.buzzer_on) buzzer_on(); else buzzer_off();
   audio_say(result.feedback.voice_text);
   ```
4. 语音 `audio_say` 当前为占位（蜂鸣表示有语音）；真实 TTS 可替换为云端返回音频流或离线 TTS 模型，接口不变。
5. 引脚全部来自 `config.h`，改线只动 config。
