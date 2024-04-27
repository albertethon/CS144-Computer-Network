Lab 6 Writeup
=============

My name: [your name here]

My SUNet ID: [your sunetid here]

This lab took me about [n] hours to do. I [did/did not] attend the lab session.

Program Structure and Design of the Router:
新增两个数据结构:

1. _next_hop  
    存子网/掩码长度 到 下一跳IP的键值对

2. _next_hop2interface  
    存下一跳IP对应的接口地址索引

在添加route规则时, 函数参数有

```cpp
const uint32_t route_prefix,
const uint8_t prefix_length,
const optional<Address> next_hop,
const size_t interface_num
```

生成掩码与子网的方式为:

```cpp
    uint32_t mast_code = ~((1ul << (32 - prefix_length)) - 1);
    uint32_t rout_net = route_prefix & mast_code;
```

这里是对ulong形式的1移位, 如果是4字节的1左移32位会重新变成1, 最终得到32位掩码  
相与得到子网

Implementation Challenges:
[]

Remaining Bugs:
 最终用DebugAsan 来测了一下发现有内存泄漏问题，但不知道怎么排查

- Optional: I had unexpected difficulty with: [describe]

- Optional: I think you could make this lab better by: [describe]

- Optional: I was surprised by: [describe]

- Optional: I'm not sure about: [describe]
