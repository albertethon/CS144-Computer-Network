Lab 0 Writeup
=============

My name: [your name here]

My SUNet ID: [your sunetid here]

This lab took me about [n] hours to do. I [did/did not] attend the lab session.

My secret code from section 2.1 was: [code here]

- Optional: I had unexpected difficulty with: 

    一开始采用ring buffer的数据结构来组织byte stream, 需要维护一个string和两个指针, 以及空flag,
    写指针指向下一个写入的位置, 读指针指向下一个读位置
    在两指针重叠的判空, 与先后顺序处理上非常麻烦,
    该方案经过一段时间调整仍fail了两个测试点, 为避免后续测试引发更多错误,
    以KISS原则采用了简单的deque结构, 抛弃了读写指针的维护

- Optional: I think you could make this lab better by: [describe]

- Optional: I was surprised by: 
    deque 虽然简单但性能不一定比string差, 因为string方案有大量的分支跳转


- Optional: I'm not sure about: [describe]
