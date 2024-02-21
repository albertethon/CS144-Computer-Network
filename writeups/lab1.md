Lab 1 Writeup
=============

My name: [your name here]

My SUNet ID: [your sunetid here]

This lab took me about [n] hours to do. I [did/did not] attend the lab session.

Program Structure and Design of the StreamReassembler:
将data分为output, assembled, unassembled三类, 维护next_unassembled下标指向下一个未assembled的字符位置, endptr指向设定为eof位置
assembled字符装入assembled map
un_assembled字符在_datas map中

push流程是先将所有字符压入_datas, 做好截断, 然后过一遍merge, 将边界重合的字符都合并
合并后更新un_assembled位置, 并根据output是否空余进行write.

注意每个写入都要对capacity--,(比如一些特殊情况的字符插入可能会忘) 否则会出现意想不到的错误
Implementation Challenges:
[]

Remaining Bugs:
[]

- Optional: I had unexpected difficulty with: [describe]

- Optional: I think you could make this lab better by: [describe]

- Optional: I was surprised by: [describe]

- Optional: I'm not sure about: [describe]

b stream_reassembler.cc
