#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

/**
 * @brief   1. 如有必要，设置ISN, 并跟踪ISN，
                进行32b wrapped seqnos与64b的转换
 *          2. 将数据与eof标志push to StreamReassemble,
                并对序列号进行解析(StreamReassemble期望从0开始)
 *
 * @param seg   TCP包
 */
void TCPReceiver::segment_received(const TCPSegment &seg) {
    const TCPHeader &header = seg.header();
    // 判断当前状态： LISTEN, SYN_RECV, FIN_RECV
    if (!is_syn_recv) {
        // 现在是LISTEN, 只收SYN包
        if (!header.syn)
            return;
        is_syn_recv = true;
        _isn = header.seqno;
    }
    const Buffer &buffer = seg.payload();

    uint64_t seq_start = unwrap(header.seqno, _isn.value(), get_checkpoint() + 1);
    // push是从buffer开始的，当前包 含SYN时，直接push
    // 不包含SYN时，需要对计算索引-1
    seq_start -= (!header.syn);
    _reassembler.push_substring(buffer.copy(), seq_start, header.fin);
}

/**
 * @brief 返回一个ACK号，窗口的左边缘，如果ISN未设置返回空
 *          窗口的左边缘即下一个unassemble的位置
 *
 * @return optional<WrappingInt32> ACK号
 */
optional<WrappingInt32> TCPReceiver::ackno() const {
    if (is_syn_recv) {
        optional<WrappingInt32> retval;

        // absolute 序号与stream的序号差1
        //
        retval.emplace(WrappingInt32({wrap(get_checkpoint() + 1, _isn.value())}));

        return retval;
    }
    return nullopt;
}

/**
 * @brief   1. capacity占满的情况，此时返回assembled的第一个字符
 *          2. capacity未满的情况，此时返回unassembled的第一个字符，表示从此处没收到。
 *          总之就是已写入字节数的下一个
 *
 * @return size_t
 */
size_t TCPReceiver::get_checkpoint() const {
    return _reassembler.stream_out().bytes_written() + _reassembler.stream_out().input_ended();
}

/**
 * @brief 窗口大小，是第一个unassembled 到第一个unacceptable的距离
 *
 * @return size_t
 */
size_t TCPReceiver::window_size() const { return _reassembler.stream_out().remaining_capacity(); }
