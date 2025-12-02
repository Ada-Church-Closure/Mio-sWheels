# 手写简单线程池

## 线程池基本参数?

核心 最大 阻塞队列模型 拒绝策略问题 超时.

## 你怎么理解拒绝策略?

本质上我们只希望core thread来执行任务.

## SHUTDOWN and SHUTDOWNNOW?

给所有正在运行的线程发interrupt信号或者是直接结束.