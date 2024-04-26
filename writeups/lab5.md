Lab 5 Writeup
=============

My name: [your name here]

My SUNet ID: [your sunetid here]

This lab took me about 8 hours to do. I [did/did not] attend the lab session.

Program Structure and Design of the NetworkInterface:

### 缩写


//本实验采用IPv4协议故这里协议地址均指IP地址

Hardware length (HLEN): 每种硬件地址的字节长度，一般为6（以太网）  
Protocol length (PLEN): 每种协议地址的字节长度，一般为4（IPv4）  
Optional: 1为ARP请求，2为ARP应答，3为RARP请求，4为RARP应答
Sender hardware address (SHA):  n个字节，n由硬件地址长度得到，一般为发送方MAC地址
Sender protocol address (SPA):  m个字节，m由协议地址长度得到，一般为发送方IP地址
Target hardware address (THA):  n个字节，n由硬件地址长度得到，一般为目标MAC地址
Target protocol address (TPA):  m个字节，m由协议地址长度得到，一般为目标IP地址

### 报文格式

ethernet 上的包分为header与payload,  
header包含source和下一跳MAC, 以及一个type表示改包是IP还是ARP  

payload若存ARP包则格式如缩写处表示， 否则就是一个IPDatagram包

### ARP 探测

IPv4 中ARP探测:  

    SHA: sender MAC
    SPA:0  
    THA:0  
    TPA: 待探测IP

如果网络上的某些主机将 TPA 视为自己的，它将回复探测（通过探测主机的 SHA），从而通知探测主机地址冲突。
相反，如果没有主机将该 IPv4 地址视为自己的地址，则不会有回复。当发送了几个探测无人回复时可以占有一个MAC地址。

### 框架结构

`send_datagram(const InternetDatagram &dgram, const Address &next_hop)`

任务：将其由IP数据包转换为以太网帧

* 如果ARP缓存有next_hop, 则将payload 序列化后打包直接发送  
* 如果没有则广播next_hop MAC的ARP request, 并记录IP datagram及其等待时间  
    1. 在收到arp reply后发送 IP datagram
    2. 过期后再发arp request

`optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame)`

任务：Ethernet 帧到达时调用, 无视目标MAC和当前不同的包(除了广播)

* 若inbound frame是IPv4, payload解析为InternetDatagram, 成功(parse返回NoError) 将生成的InternetDatagram返回
* 若inbound frame是ARP, 解析payload为ARPMessage, 成功则记住sender IP-> MAC 30秒
    从requests和replies中学习映射  
    如果是要求我们IP地址的ARP请求, 发送适当的ARP回复  

`tick`

任务：计时，并告知ip->MAC映射过期，删除

Implementation Challenges:
[]

Remaining Bugs:
[]

- Optional: I had unexpected difficulty with: [describe]

- Optional: I think you could make this lab better by: [describe]

- Optional: I was surprised by: [describe]

- Optional: I'm not sure about: [describe]
