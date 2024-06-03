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

* 从来没发送过 FIN  
* 输入字节流处于 EOF，必须是处于eof,即_stream.eof(),
        之前我写_stream.input_ended()，这个表示曾输入过eof, 但可能buffer里还有字节未传输出去
        debug了一天！  
* window 减去 payload与syn标志大小后，仍然可以存放下 FIN

优化：  
    1. 对stream_reassembler进行优化，将存储assemble的结构由map改为string(其本身是连续的)  
    2. 更改peek_out,pop_out逻辑, 删去分支判断, 删去peek中循环部分，直接用string的构造函数, 0.6Gb/s提升至1.0Gb/s  
    3. 将write部分由for循环 push_back, 改为insert到end, 1.0Gb/s提升至1.38Gb/s  
    4. lab1中stream_reassembler 对于map的范围删除erase部分, 如果采用逐个比较是否覆盖，需要删除m个节点是O(m logn)的复杂度， 如果用upper_bound和erase(first, last)来删除, 则复杂度分别为 O(logn) 与 O(logn + k)
    5. 将push_string中存储 assemble的结构用buffer代替, Debug版本速度提升至 2.56Gb/s

修复BUG：

测试fsm_stream_reassembler_many会出现double free问题，这个问题debug了我两天， 问题出在assembled 压进out_put后， remove_prefix时会出现double free

valgrind查 free 发现有一个地方free了一个大小为0的block size. 估计是这里 double free.

查 alloc这块内存的源码， 问题出在__push_substring中data 覆盖assemble与 unassemble场景下

这里我是打算声明一个string变量存储被截断的data, 通过右值引用接到assemble的尾部。原本如此做

```cpp
string&& _append_str = move(string().assign(data.substr(next_unassembled - index, _data_write_len)));//error

_assembled_strs.append(Buffer(std::move(_append_str)));
```

发现在这里居然会出现一个free， 如果抛弃右值引用将上述右值全部填入append参数，即可通过

```cpp
_assembled_strs.append(
Buffer(move(string().assign(
data.substr(next_unassembled - index, _data_write_len)))));
```

这里我猜测一个bug原因:  
右值引用本身也是作为一个左值存在, 具有作用域, 在右值诞生时其声明周期与该右值引用相同。
即使被move到一个外部结构中也无法改变这个右值的生命周期。
故在离开作用域后该右值触发了一个析构。  
这就是提前析构，那第二次外部结构正常析构时就肯定会导致double free了

Remaining Bugs:

lab4 的部分测试Test145~153无法通过，查找网上的其他仓库运行也无法通过， 逛论坛有人提出可能是网速和性能问题，暂搁置


- Optional: I had unexpected difficulty with: [describe]

- Optional: I think you could make this lab better by: [describe]

- Optional: I was surprised by: [describe]

- Optional: I'm not sure about: 