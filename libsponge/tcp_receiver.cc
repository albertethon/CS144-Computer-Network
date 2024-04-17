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
    const Buffer    &buffer = seg.payload();
    if(header.syn){
        _isn.emplace(header.seqno);
    }

    if(this->_isn.has_value()){
        uint64_t seq_start = unwrap(header.seqno,_isn.value(), get_checkpoint());
        //push是从buffer开始的，当前包 含SYN时，直接push
        //不包含SYN时，需要对计算索引-1
        seq_start -= (!header.syn);
        string buffer_str = string(buffer.str().data(),buffer.size());
        _reassembler.push_substring(std::move(buffer_str), seq_start, header.fin);

    }
}

/**
 * @brief 返回一个ACK号，窗口的左边缘，如果ISN未设置返回空
 *          窗口的左边缘即下一个unassemble的位置
 * 
 * @return optional<WrappingInt32> ACK号
 */
optional<WrappingInt32> TCPReceiver::ackno() const { 
    if(_isn.has_value()){
        optional<WrappingInt32> retval;

        // absolute 序号与stream的序号差1
        // 
        retval.emplace(WrappingInt32({wrap(get_checkpoint()+1, _isn.value())}));

        return retval;
    }
    return optional<WrappingInt32>();
}

/**
 * @brief   1. capacity占满的情况，此时返回assembled的第一个字符
 *          2. capacity未满的情况，此时返回unassembled的第一个字符，表示从此处没收到。
 *
 * @return size_t
 */
size_t TCPReceiver::get_checkpoint() const{
    size_t checkpoint;
    //XXX: 可优化，这里assembled_bytes是统计所有的assembled 字节，其实只用知道它存在就行
    if(_reassembler.assembled_bytes()>0){
        checkpoint = _reassembler.get_first_assembled();
    }else{
        checkpoint = _reassembler.get_next_unassembled();
    }
    return checkpoint;
}

/**
 * @brief 窗口大小，是第一个unassembled 到第一个unacceptable的距离
 * 
 * @return size_t 
 */
size_t TCPReceiver::window_size() const { 
    return _reassembler.stream_out().remaining_capacity();
}
