Lab 2 Writeup
=============

My name: 施佐烨

My SUNet ID: 

This lab took me about [n] hours to do. I [did not] attend the lab session.

I worked with or talked about this assignment with: [please list other sunetids]

Program Structure and Design of the TCPReceiver and wrap/unwrap routines:

1. WrappingInt32 根据isn与绝对序号seqno, 转换得到seq( 高位与0)

2. unwrap, 根据seq, isn, 得到绝对序号seqno

3. 转换所得为绝对序号，可能包含SYN,FIN包的序号，需要分别讨论来转换到stream的序号
Implementation Challenges:
接收的字符串中存在'\0'会导致截断，push一半给reassembler, 需要使用构造函数指定对字符串的长度进行构造

Remaining Bugs:
[]

- Optional: I had unexpected difficulty with: [describe]

- Optional: I think you could make this lab better by: [describe]

- Optional: I was surprised by: [describe]

- Optional: I'm not sure about: [describe]
