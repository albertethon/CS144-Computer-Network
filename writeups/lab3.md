Lab 3 Writeup
=============

My name: [your name here]

My SUNet ID: [your sunetid here]

This lab took me about [n] hours to do. I [did/did not] attend the lab session.

I worked with or talked about this assignment with: [please list other sunetids]

Program Structure and Design of the TCPSender:
    1. 需要跟踪receiver 窗口大小
    2. 从ByteStream组织TCP Segment, 并发送它们，尽可能填充窗口。
    3. 跟踪发送但未收到ACK段：outstanding segments
    4. outstanding segments超时重传

如何处理超时重传：
    1. 定时调用tick(millionseconds elapsed) 方法
    2. TCPSender构造时赋予一个初始RTO(retransmission timeout), 用于outstanding段等待时间，RTO随时间变化，但初始RTO不变
    3. 实现timer, 在特定时间启动, 并在RTO过期时结束(利用tick计时)
    4. 每次发送段启动计时器
    5. outstanding 被ACK时停止计时器
    6. 如果过期, 则
        a. 重传最早segment
        b. 若窗口大小非零:
            i. 跟踪连续重传次数，并自增
            ii. RTO翻倍(exponential backoff), 属于拥塞控制
        c. 重启计时器
    7. Sender收到Receiver的ACK
        a. 设置RTO回初始值
        b. 当存在outstanding data, 重启计时器
        c. 连续重传计数重置为0
misc:
    * 将segment放入segments_out视为发送
    * 接收ACK前默认窗口大小1字节
    * 如果仅确认部分seg, 等outstanding对应seg所有序列都被ack再删，不用裁剪
    * 只需单独跟踪每个outstanding seg
    * 不存empty_seg
Implementation Challenges:
对于多次传输的ACK, 和过期的ACK处理，注意检查ACK号处于outgoing范围内，且当所有包都被ACK时要及时停止计时器

Remaining Bugs:
修复了lab0的BUG， read超过buffer时限制为buffer.size

- Optional: I had unexpected difficulty with: [describe]

- Optional: I think you could make this lab better by: [describe]

- Optional: I was surprised by: [describe]

- Optional: I'm not sure about: [describe]
