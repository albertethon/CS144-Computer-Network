#include "tcp_connection.hh"

#include <cassert>
#include <iostream>
#include <limits>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

// 设置RST状态并根据需要发射RST包
void TCPConnection::_set_rst_state(bool send_rst) {
    if (send_rst) {
        TCPSegment _rst_seg;
        _rst_seg.header().rst = true;
        _segments_out.push(_rst_seg);
    }
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _linger_after_streams_finish = false;
    _is_active = false;
    return;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    auto _sender_state = TCPState::state_summary(_sender);
    auto _recv_state = TCPState::state_summary(_receiver);

    // reset time
    _time_since_last_segment_received = 0;
    _receiver.segment_received(seg);  // 收包

    bool need_send_ack = seg.length_in_sequence_space();  // 对于所有SYN,FIN以及含有payload的数据都要ACK

    //----------------ERROR状态或接收RST----------------------//
    // 错误状态直接结束
    if (_sender_state == TCPSenderStateSummary::ERROR || _recv_state == TCPReceiverStateSummary::ERROR) {
        return;
    }
    // 当接收的包含有RST标志
    if (seg.header().rst) {
        _set_rst_state(false);
        return;
    }

    //----------------收到非当前状态应收packets---------------------//
    // recv状态
    if (_recv_state == TCPReceiverStateSummary::LISTEN) {
        // LISTEN阶段收到FIN,ACK包无视
        if (!seg.header().syn) {
            return;
        }
    }

    //----------------收到ACK，更新sender状态----------------------//
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    //---------------------握手逻辑----------------------//
    if (seg.header().syn && _recv_state == TCPReceiverStateSummary::LISTEN &&
        _sender_state == TCPSenderStateSummary::CLOSED) {
        connect();
        return;
    }
    //---------------------挥手逻辑----------------------//
    // 收到第一次挥手：收到的包有FIN，且local之前未发过FIN, 则挥手时采用被动关闭，不等
    //              如果没数据发ACK空包，否则结合数据发过去
    if (seg.header().fin && _sender_state == TCPSenderStateSummary::SYN_ACKED) {
        _linger_after_streams_finish = false;
        // _sender.fill_window();
    }
    // 收到第三次挥手：发ACK空包过去, FIN为真，自动发ACK

    // 收到第四次挥手：关闭
    if (TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED && !_linger_after_streams_finish) {
        _is_active = false;
        return;
    }
    //--------------------------------------------------//

    if (need_send_ack && _sender.segments_out().empty()) {
        _sender.send_empty_segment();
    }
    // send the segment
    _transmit_seg_with_ack_syn();
}

bool TCPConnection::active() const { return _is_active; }

size_t TCPConnection::write(const string &data) {
    // when tcp connection died
    if (!_is_active) {
        return 0;
    }

    size_t write_size = _sender.stream_in().write(data);
    _sender.fill_window();

    _transmit_seg_with_ack_syn();
    return write_size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    assert(_sender.segments_out().empty());
    _sender.tick(ms_since_last_tick);
    // 当重传次数超过最大值，则发送RST包并abort当前connection
    if (_sender.consecutive_retransmissions() >
        _cfg.MAX_RETX_ATTEMPTS) {  // 用当前对象_cfg, 而非TCPConfig, 后者无法在外部更改
        while (!_sender.segments_out().empty()) {
            _sender.segments_out().pop();
        }
        _set_rst_state(true);
        return;
    }

    _transmit_seg_with_ack_syn();

    _time_since_last_segment_received += ms_since_last_tick;

    // 主动关闭方在sender FIN_ACK 且 receiver FIN_RCVD后等待 10 * rto关闭
    if (_linger_after_streams_finish && TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED &&
        TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        _time_since_last_segment_received >= (10 * _cfg.rt_timeout)) {
        _is_active = false;  // 时间不够则继续活跃
        _linger_after_streams_finish = false;
    }
}

void TCPConnection::end_input_stream() {
    // 第一次，第三次挥手逻辑相同
    _sender.stream_in().end_input();
    _sender.fill_window();
    _transmit_seg_with_ack_syn();
}

void TCPConnection::connect() {
    // 第一次握手, send SYN+ACK
    _sender.fill_window();
    _is_active = true;
    _transmit_seg_with_ack_syn();
}

void TCPConnection::_transmit_seg_with_ack_syn() {
    TCPSegment sender_seg;
    // sender 有待发送包， 发完
    while (!_sender.segments_out().empty()) {
        sender_seg = _sender.segments_out().front();
        _sender.segments_out().pop();

        // 准备发送的ackno和window_size 由 receiver提供
        if (_receiver.ackno().has_value()) {
            sender_seg.header().ackno = _receiver.ackno().value();
            sender_seg.header().ack = true;
            sender_seg.header().win = _receiver.window_size() > UINT16_MAX ? UINT16_MAX : _receiver.window_size();
        }

        _segments_out.push(sender_seg);
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            _set_rst_state(true);
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
