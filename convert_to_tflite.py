#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
convert_to_tflite.py —— 盲道引导系统「端侧 AI」模型转换脚本
============================================================
把训练好的检测模型（PyTorch / YOLO / 任意导出 ONNX）转换为 ESP32-S3 上
TFLite Micro 可直接加载的 **int8 量化** detect.tflite。

⚠️ 本机（用户机器）无 GPU / 无外网，以下命令需在「有 GPU + 联网」的机器上跑。

输出约定（必须严格遵守，ondevice_detect.cpp 按此解码）：
    输入  : [1, H, W, 3] uint8 RGB（H=W=96 或 160，见 config.h LOCAL_DETECT_W/H）
    输出  : [1, N, 6] float/uint8，每框 6 值：
            [x_center, y_center, width, height, class_id, confidence]
            坐标均归一化 0~1；class_id ∈ {0..4}（0=盲道 1=斑马线 2=井盖 3=障碍 4=其他）
            推理后处理（NMS/阈值）在 ESP32 端完成，故本脚本只做图级别量化导出。

类别顺序（CLASS_*）：
    0 = blindpath（盲道）  1 = zebra（斑马线）  2 = manhole（井盖）
    3 = obstacle（障碍）   4 = other/background（其它/背景，可选）

用法：
    python convert_to_tflite.py --src model.onnx --out detect.tflite --size 96
依赖：
    pip install onnx onnx-tensorrt tf2onnx tensorflow
"""
import argparse, sys

# ---------------- 1. 导出 ONNX -> TFLite（示例：用 tf 的 from_onnx） ----------------
def export_onnx_to_tflite(src, out, size):
    import onnx
    # 方案A：如果我们用 PyTorch 训练的 YOLO-nano，先 torch.onnx.export（见下脚本片段）
    # 方案B：已有 ONNX，用 tf2onnx 转 SavedModel 再转 TFLite
    try:
        import tf2onnx, tensorflow as tf
    except Exception as e:
        print("[ERR] 需要 tf2onnx + tensorflow：pip install tf2onnx tensorflow"); sys.exit(1)

    print(f"[1/3] 加载 ONNX {src} ...")
    import onnxruntime as ort
    # 用 from_onnx 生成 TFLite（tf2onnx 支持 onnx->tf->tflite 链路）
    import subprocess
    # onnx → tflite 直接命令行（tf2onnx + TFLiteConverter）
    import tensorflow as tf
    from onnx_tf.backend import prepare  # 需 onnx-tf
    onnx_model = onnx.load(src)
    tf_rep = prepare(onnx_model)
    tf_rep.export_graph("saved_model_tmp")
    print(f"[2/3] 量化导出 int8 TFLite --input {size}x{size} ...")
    converter = tf.lite.TFLiteConverter.from_saved_model("saved_model_tmp")
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = _representative_dataset(size)
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.uint8
    converter.inference_output_type = tf.uint8
    tflite_model = converter.convert()
    with open(out, "wb") as f:
        f.write(tflite_model)
    print(f"[3/3] 已写出 {out} ({len(tflite_model)} bytes)")

def _representative_dataset(size):
    import numpy as np
    # 用少量真实盲道/斑马线/井盖/障碍图片（RGB uint8 归一化到 0~255）做校准
    # 这里给一个占位：实际请替换为你的校准图目录（见 README）。
    for _ in range(20):
        yield [ (np.random.rand(1, size, size, 3) * 255).astype(np.uint8) ]

# ---------------- 2. PyTorch YOLO-nano 训练/导出样例（参考） ----------------
def example_pytorch_export():
    """
    在训练机执行（示意，需要根据你的网络结构实现 forward）：
        import torch
        model = MyYOLONano(num_classes=5)   # 类别 0~4
        model.load_state_dict(torch.load("best.pt"))
        model.eval()
        dummy = torch.randn(1, 3, 96, 96)
        torch.onnx.export(model, dummy, "model.onnx",
                          input_names=["input"], output_names=["detections"],
                          opset_version=13,
                          dynamic_axes={"detections": {1: "N"}})
    然后调用 export_onnx_to_tflite("model.onnx", "detect.tflite", 96)
    """
    print("[INFO] 见函数注释：在训练机用 torch.onnx.export 导出 model.onnx")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True, help="训练导出模型(.onnx)")
    ap.add_argument("--out", default="detect.tflite")
    ap.add_argument("--size", type=int, default=96, help="检测张量边长(96/160)")
    args = ap.parse_args()
    export_onnx_to_tflite(args.src, args.out, args.size)

if __name__ == "__main__":
    main()
