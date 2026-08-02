"""
rate_limiter.py —— 令牌桶限流（阶段四：防止异常调用耗尽 API 额度）
每设备每秒最多 1 次请求。
"""
import time

class TokenBucket:
    def __init__(self, rate: float, capacity: float):
        self.rate = rate          # 令牌生成速率（个/秒）
        self.capacity = capacity  # 桶容量
        self.tokens = capacity
        self.last = time.time()

    def consume(self, n: int = 1) -> bool:
        now = time.time()
        self.tokens = min(self.capacity, self.tokens + (now - self.last) * self.rate)
        self.last = now
        if self.tokens >= n:
            self.tokens -= n
            return True
        return False
