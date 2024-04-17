#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    // +上isn并保留低32位
    return WrappingInt32{isn + static_cast<uint32_t>(n)};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number, 距离绝对seq最近的点
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // n-isn 返回的是当前字节到isn 距离, checkpoint是isn对应在绝对seqno的位置下标
    constexpr const long INT32_RANGE = 1l << 32;
    uint64_t offset = (n - isn + (INT32_RANGE)) % (INT32_RANGE);

    /**
     * @brief 这里要求转换后的绝对seq距离checkpoint最近, 设w为1个循环(即1l<<32),
        真实的offset可以在kw+offset处,(k+1)w+offset处
        取checkpoint近处，即(k+1/2)w+offset < checkpoint 则取右(k+1)，反之左侧(k)
        经过代数变换得 checkpoint-offset + 1/2 * w的值整除w，即为wraps的循环数
     */
    if (offset < checkpoint) {
        uint64_t real_checkpoint = checkpoint - offset + (1ul << 31);
        uint32_t wraps = real_checkpoint / INT32_RANGE;
        return wraps * INT32_RANGE + offset;
    } else {
        return offset;
    }
}
