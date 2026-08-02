# -*- coding: utf-8 -*-
"""
fsm_model.py —— 盲道引导状态机(三帧滑动窗口防抖)的 Python 参考实现
用于阶段七单元测试。算法与 06_核心业务逻辑模块/guidance_fsm.cpp 完全一致，
便于在无硬件环境下验证防抖逻辑正确性。
"""


class GuidanceFSM:
    STATES = ["FOLLOWING", "OFFSETLEFT", "OFFSETRIGHT", "TURNING", "ENDDETECTED"]

    def __init__(self):
        self.current = "FOLLOWING"
        self._window = ["FOLLOWING", "FOLLOWING", "FOLLOWING"]
        self._idx = 0

    def update(self, candidate):
        # candidate: 本帧候选状态字符串；若未检测到盲道则归为 FOLLOWING
        if candidate not in self.STATES:
            candidate = "FOLLOWING"
        self._window[self._idx] = candidate
        self._idx = (self._idx + 1) % 3

        counts = {s: 0 for s in self.STATES}
        for s in self._window:
            counts[s] += 1
        best, maxc = self.current, 0
        for s in self.STATES:
            if counts[s] > maxc:
                maxc, best = counts[s], s
        if maxc >= 2 and best != self.current:   # 三帧>=2相同才切换
            self.current = best
        return self.current
