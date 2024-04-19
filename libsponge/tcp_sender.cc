#include "tcp_sender.hh"

#include "tcp_config.hh"
#include "tcp_state.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _tcp_timer(_initial_retransmission_timeout, 0) {}

uint64_t TCPSender::bytes_in_flight() const { return _outgoing_bytes; }

/**
 * @brief 从ByteStream中组织TCP Segment, 不超过TCPConfig::MAX_PAYLOAD_SIZE
 *  使用TCPSegment::length_in_sequence_space()统计总共序号数
 *  当window为0时发送大小为1的段，从而第一时间得知更新变大的窗口，同时流量控制
 *
 */
void TCPSender::fill_window() {
    _send_window_size = _recv_window_size > 0 ? _recv_window_size : 1;
    while (_send_window_size > _outgoing_bytes) {
        TCPSegment seg;
        TCPHeader header;

        /**
         * @brief sender的状态分为CLOSED, SYN_SENT, SYN_ACKED, SYN_ACKED_EOF, FIN_SENT, FIN_ACKED
         * @note
                CLOSED:         next_seqno_absolute == 0

                SYN_SENT:       next_seqno_absolute > 0
                                && next_seqno_absolute == bytes_in_flight

                SYN_ACKED:      not eof
                                && next_seqno_absolute > bytes_in_flight

                SYN_ACKED_EOF:  eof
                                && next_seqno_absolute < bytes_written + 2

                FIN_SENT:       eof
                                && next_seqno_absolute == bytes_written + 2
                                && bytes_in_flight > 0

                FIN_ACKED:      eof
                                && next_seqno_absolute == bytes_written + 2
                                && bytes_in_flight == 0
         */

        // 装填window, CLOSE时单发一个SYN, SYN和FIN占一个window_size
        // auto max_buffer_size = min(_send_window_size, TCPConfig::MAX_PAYLOAD_SIZE);

        // 至少有一个window空间，则可以发SYN/FIN包等
        // CLOSED
        auto state = TCPState::state_summary(*this);
        if (state == TCPSenderStateSummary::CLOSED) {
            header.syn = true;
        }

        // 窗口删去outstanding 字节数和SYN包，看看剩下最多装多少payload
        auto max_payload_size = min(_send_window_size - _outgoing_bytes - header.syn, TCPConfig::MAX_PAYLOAD_SIZE);
        Buffer buffer(_stream.read(max_payload_size));

        // SYN_ACKED_EOF
        if (state == TCPSenderStateSummary::SYN_ACKED) {
            // 此时需要判断窗口是否有位置装FIN包
            if (_stream.input_ended() && _outgoing_bytes + buffer.size() < _send_window_size) {
                header.fin = true;
            }
        }

        // 装填
        header.seqno = next_seqno();
        seg.header() = header;
        seg.payload() = buffer;

        // 数据包是空的，就不发
        if (seg.length_in_sequence_space() == 0)
            break;

        _next_seqno += seg.length_in_sequence_space();      // seq自增
        _outgoing_bytes += seg.length_in_sequence_space();  // 追踪outgoing窗口
        _last_ackno = max(_last_ackno, _next_seqno);

        // 不必担心复制带来的空间浪费，这里seg.payload的buffer是共享指针管理的，只存储一份storage
        _segments_out.push(seg);
        _outstanding.push_back(seg);
        _tcp_timer.start();
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // 当收到的ackno远大于最后一个等待的ack时抛弃
    auto ack_recvd = unwrap(ackno, _isn, next_seqno_absolute());
    if (ack_recvd > _last_ackno)
        return;

    _recv_window_size = window_size;

    size_t _old_size = _outstanding.size();
    /**
     * @brief 对于ack确认的段, 可以扔了,
     *          发送应当是顺序发送， 乱序接收ACK
     *          顺序发送的seg在outstanding顺序存储，根据ACK来顺序删除，小ACK不管
     *
     * @param out_seg:_outstanding
     */
    while (!_outstanding.empty()) {
        auto out_seg = _outstanding.front();
        auto ack_wait = out_seg.header().seqno + out_seg.length_in_sequence_space();
        auto uw_ack_wait = unwrap(ack_wait, _isn, next_seqno_absolute());

        // 大于当前seq+length即可，要求该ACK处于发送范围内，否则无视（比如远大于当前的seq号，显然不能把outstanding清空）
        if (uw_ack_wait <= ack_recvd && ack_recvd <= _last_ackno) {
            _outgoing_bytes -= out_seg.length_in_sequence_space();
            _outstanding.pop_front();
        } else {
            break;
        }
    }

    // 当有outstanding包ack确认收到， 则重置计时器
    if (_old_size != _outstanding.size()) {
        _tcp_timer.set_RTO(_initial_retransmission_timeout);
        // 连续重传计数器置为0
        _RT_times = 0;

        // 还存在outstanding data就重启计时器
        if (!_outstanding.empty()) {
            _tcp_timer.restart();
        } else {
            _tcp_timer.close();
        }
    }

    // 收到ACK更新seqno, 如果是之前的ack就不更新
    _next_seqno = max(_next_seqno, unwrap(ackno, _isn, _next_seqno));
}

/**
 * @brief 在tick中处理超时重传的逻辑
 *
 * @param ms_since_last_tick
 */
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (_tcp_timer.timeOut(ms_since_last_tick)) {
        // 如果已经过期
        // 跟踪连续重传次数，自增
        ++_RT_times;
        // 如果窗口大小非0且超时，说明网络拥堵
        if (_recv_window_size > 0) {
            // RTO翻倍， 拥塞控制
            _tcp_timer.set_RTO(_tcp_timer.get_RTO() * 2);
        }
        // 发送最早的seg
        _segments_out.push(_outstanding.front());
        // 重启计时器
        _tcp_timer.restart();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _RT_times; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}
