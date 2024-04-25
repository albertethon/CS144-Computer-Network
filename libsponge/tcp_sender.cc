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
    // 至少有一个window空间，则可以发SYN/FIN包等
    _send_window_size = _recv_window_size > 0 ? _recv_window_size : 1;
    while (_send_window_size > _outgoing_bytes) {
        TCPSegment seg;
        TCPHeader &header = seg.header();
        Buffer &buffer = seg.payload();

        // 装填window,SYN和FIN占一个window_size
        if (!_is_syn_flag) {  //  CLOSE时单发一个SYN
            header.syn = true;
            _is_syn_flag = true;
        }
        header.seqno = next_seqno();  // 更新序号

        // 窗口删去outstanding 字节数和SYN包，看看剩下最多装多少payload
        auto max_payload_size = min(_send_window_size - _outgoing_bytes - header.syn, TCPConfig::MAX_PAYLOAD_SIZE);
        buffer = Buffer(_stream.read(max_payload_size));

        /**
         * @brief 读取好后，如果满足以下条件，则增加 FIN
         *  1. 从来没发送过 FIN
         *  2. 输入字节流处于 EOF，必须是处于eof,即_stream.eof(),
                之前我写_stream.input_ended()，这个表示曾输入过eof, 但可能buffer里还有字节未传输出去
                debug了一天！
         *  3. window 减去 payload与syn标志大小后，仍然可以存放下 FIN
         *
         */
        if (!_is_fin_sent && _stream.eof() && _outgoing_bytes + buffer.size() + header.syn < _send_window_size) {
            header.fin = true;
            _is_fin_sent = true;
        }

        // 数据包是空的，就不发
        if (seg.length_in_sequence_space() == 0)
            break;

        // 如果没有等待队列的数据包，则更新时间
        if (_outstanding.empty()) {
            _tcp_timer.restart();
            _tcp_timer.set_RTO(_initial_retransmission_timeout);
        }

        _next_seqno += seg.length_in_sequence_space();      // seq自增
        _outgoing_bytes += seg.length_in_sequence_space();  // 追踪outgoing窗口

        // 不必担心复制带来的空间浪费，这里seg.payload的buffer是共享指针管理的，只存储一份storage
        _segments_out.push(seg);      // 发送
        _outstanding.push_back(seg);  // 追踪

        if (seg.header().fin)
            break;  // 如果该包为结束包，则不应继续发送
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // 当收到的ackno大于最后一个等待的ack时抛弃
    const size_t ack_recvd = unwrap(ackno, _isn, next_seqno_absolute());
    if (ack_recvd > _next_seqno)
        return;

    _recv_window_size = window_size;

    const size_t _old_size = _outstanding.size();
    /**
     * @brief 对于ack确认的段, 可以扔了,
     *          发送应当是顺序发送， 乱序接收ACK
     *          顺序发送的seg在outstanding顺序存储，根据ACK来顺序删除，小ACK不管
     *
     * @param out_seg:_outstanding
     */
    while (!_outstanding.empty()) {
        const TCPSegment &out_seg = _outstanding.front();
        const auto ack_wait_wrap = out_seg.header().seqno + out_seg.length_in_sequence_space();
        const size_t ack_wait = unwrap(ack_wait_wrap, _isn, next_seqno_absolute());

        // 大于当前seq+length即可，要求该ACK处于发送范围内，否则无视（比如远大于当前的seq号，显然不能把outstanding清空）
        if (ack_wait <= ack_recvd) {
            _outgoing_bytes -= out_seg.length_in_sequence_space();
            _outstanding.pop_front();
        } else {
            break;
        }
    }

    // 当有outstanding包ack确认收到， 则重置计时器
    if (_old_size != _outstanding.size()) {
        _tcp_timer.set_RTO(_initial_retransmission_timeout);

        // 还存在outstanding data就重启计时器
        if (!_outstanding.empty()) {
            _tcp_timer.restart();
        } else {
            _tcp_timer.close();
        }
    }
    // 连续重传计数器置为0,只要收到ACK就应当置0
    _RT_times = 0;

    fill_window();
}

/**
 * @brief 在tick中处理超时重传的逻辑
 *
 * @param ms_since_last_tick
 */
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_outstanding.empty() && _tcp_timer.timeOut(ms_since_last_tick)) {
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
