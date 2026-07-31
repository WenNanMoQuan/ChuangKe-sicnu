# -*- coding: utf-8 -*-
"""
test_fsm.py —— 状态机防抖单元测试（pytest）
阶段七：功能测试覆盖盲道引导状态转换路径
运行： pytest test_fsm.py -v
"""
import pytest
from fsm_model import GuidanceFSM


def test_following_stable():
    fsm = GuidanceFSM()
    for _ in range(5):
        assert fsm.update("FOLLOWING") == "FOLLOWING"


def test_single_glitch_ignored():
    """偶发一帧误识别不应切换状态（三帧>=2才切换）"""
    fsm = GuidanceFSM()
    fsm.update("FOLLOWING")
    fsm.update("OFFSETLEFT")   # 单帧误识别
    fsm.update("FOLLOWING")
    assert fsm.current == "FOLLOWING"


def test_two_of_three_switches():
    """连续两帧相同则切换"""
    fsm = GuidanceFSM()
    assert fsm.update("TURNING") == "FOLLOWING"   # 仅1帧，不切
    assert fsm.update("TURNING") == "TURNING"     # 累计2帧，切换


def test_end_detected_transition():
    fsm = GuidanceFSM()
    seq = ["FOLLOWING", "ENDDETECTED", "ENDDETECTED", "ENDDETECTED"]
    out = [fsm.update(s) for s in seq]
    assert out[-1] == "ENDDETECTED"


def test_unknown_candidate_defaults_following():
    fsm = GuidanceFSM()
    assert fsm.update("GARBAGE") == "FOLLOWING"
