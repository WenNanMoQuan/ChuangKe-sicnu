# -*- coding: utf-8 -*-
"""
protocol_fields.py —— 与 01_数据协议与引脚配置/protocol.h 的 K_* 键名保持一一对应
用于云端/测试侧校验返回 JSON 字段是否与端侧解析契约一致（三者必须相同：
protocol.h 的 K_* = 08 返回 JSON 键 = 09 提示词输出字段）。
"""
# 顶层
EXPECTED_KEYS = ["blind_path", "obstacles", "feedback", "degraded"]
# 可选但推荐的顶层字段（驱动端侧天气模式）
OPTIONAL_KEYS = ["weather"]
# blind_path 子字段
BLIND_PATH_KEYS = ["detected", "offset_direction", "offset_angle", "status"]
# feedback 子字段
FEEDBACK_KEYS = ["vibration", "buzzer", "led_color", "voice_text"]
# status 合法枚举
STATUS_VALUES = ["FOLLOWING", "OFFSETLEFT", "OFFSETRIGHT", "TURNING", "ENDDETECTED"]
# weather 合法枚举（与 01/protocol.h 的 WeatherCond 映射一致）
WEATHER_VALUES = ["clear", "rain", "snow", "fog"]
# weather 字段键名（与 protocol.h 的 K_WEATHER 一致）
K_WEATHER = "weather"
