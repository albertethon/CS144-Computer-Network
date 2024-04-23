Lab 4 Writeup
=============

My name: [your name here]

My SUNet ID: [your sunetid here]

This lab took me about [n] hours to do. I [did/did not] attend the lab session.

I worked with or talked about this assignment with: [please list other sunetids]

Program Structure and Design of the TCPConnection:

1. 接收segments
    TCPConnection 调用segment_received 接收 TCPsegments ，调用该方法时查看segment 并：
    * 当RST 设置，将inbound & outbound设置为error state并永久终止链接
    * 把seg给TCPReceiver, 包括其中的seqno, SYN, payload, FIN
    * 当ACK被设置, 给TCPSender 设置ackno, window_size
    * 若传入数据段占用seq, 则TCPConnection至少发一个数据段恢复以确认更新ackno和window_size

2. 发送segments
    * 任何时候TCPSender将Segment推送至队列，并设置seqno, SYN, payload and FIN
    * 在发送segment前，TCPConnection访问TCPReceiver的ackno,window_size, 并依据此设ACK flag

3. time计时
    * 告知TCPSender时间流逝
    * 重传超过TCPConfig::MAX_RETX_ATTEMPS则终止链接并发送RST segment
    * 如果必要，干净的结束连接
4. 如何结束？
    * 非正常关闭，发送RST包，然后给本地的sender, receiver设error
    * 正常关闭需要保证：
        1. inbound 流被彻底assembled并ended
        2. outbound 流ended并被彻底sent(包括FIN)给remote
        3. outbound 流被全部ACK确认
        4. local 确认 remote端已经满足了条件3. 这可以通过两种方案实现：  
            a. 在两个流结束后等待:  
                等待一段时间以确保remote 端不再重发任何包。意味着结束  
            b. 被动关闭：当1~3为真，且local确认remote满足3. 这是因为remote先发送FIN包，被动关闭方在发送第二个FIN包并被确认时可以确认remote方已经满足了3

Implementation Challenges:

注意不对纯ACK进行ACK，其他的包一律ACK
sender在接收到ACK时要检查是否有因为堵塞未发送的包(outgoing占满)， 当留出空要立即发送，这个工作应由sender完成
修复tcp_sender BUG:
    1. ack接收到后无论如何都应当设置重传计数器，而不是看是否成功ACK了outgoing packets
    2. 读取好后，如果满足以下条件，则增加 FIN
         *从来没发送过 FIN
         * 输入字节流处于 EOF，必须是处于eof,即_stream.eof(),
                之前我写_stream.input_ended()，这个表示曾输入过eof, 但可能buffer里还有字节未传输出去
                debug了一天！
         * window 减去 payload与syn标志大小后，仍然可以存放下 FIN

Remaining Bugs:
[]

- Optional: I had unexpected difficulty with: [describe]

- Optional: I think you could make this lab better by: [describe]

- Optional: I was surprised by: [describe]

- Optional: I'm not sure about: [describe]
