#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

bool StreamReassembler::overlapped(const pair<size_t, string> &s1, const pair<size_t, string> &s2) {
    return (max(s1.first + s1.second.length(), s2.first + s2.second.length()) - min(s1.first, s2.first)) <=
           (s1.second.length() + s2.second.length());
}

void StreamReassembler::merge_all() {
    if (_datas.empty())
        return;

    auto pre = _datas.begin();
    auto next = pre;
    if (next == _datas.end())
        return;
    ++next;

    while (next != _datas.end()) {
        if ((pre->first + pre->second.length()) == next->first) {
            pre->second.append(next->second);
            _datas.erase(next->first);
            next = pre;
            ++next;
        } else {
            pre = next;
            ++next;
        }
    }
}

void StreamReassembler::flush_assembled() {
    // 当output有空余且assembled存在字符
    while (_output.remaining_capacity() > 0 && !_assembled_strs.empty()) {
        const unsigned long _next_id = _assembled_strs.begin()->first;
        const auto &_next_str = _assembled_strs.begin()->second;
        const size_t write_bytes = _output.write(_next_str);

        // add the capacity
        _remain_capacity += write_bytes;

        // remove from assembled strs
        if (write_bytes == _next_str.length())
            _assembled_strs.erase(_next_id);
        else {
            _assembled_strs[_next_id + write_bytes] = _next_str.substr(write_bytes);
            _assembled_strs.erase(_next_id);
        }
    }
}

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity)
    , _capacity(capacity)
    , _remain_capacity(capacity)
    , _endptr(-1)
    , _datas()
    , _assembled_strs()
    , next_unassembled(0) {}

void StreamReassembler::__push_substring(const string &data, const size_t index) {
    // overlap的部分assembled, 或空的情况跳过，或index大于当前可接受的最后插入点
    if (data.empty() || index + data.length() <= next_unassembled || index >= _output.bytes_written() + _capacity)
        return;

    size_t now = next_unassembled;
    // overlap部分横跨assembled, un_assembled
    if (index < next_unassembled && next_unassembled < (index + data.length())) {
        // create data in un_assembled
        _datas[now] = data[now - index];
        --_remain_capacity;
        auto prev_iter = _datas.find(now);
        ++now;
        // find first key iter bigger than index
        auto next = _datas.upper_bound(index);

        while (now < index + data.length() && _remain_capacity > 0) {
            if (next == _datas.end()) {
                // no next iter
                prev_iter->second.push_back(data[now - index]);
                --_remain_capacity;
                ++now;
            } else if (now < next->first) {
                // between prev and next
                prev_iter->second.push_back(data[now - index]);
                --_remain_capacity;
                ++now;
            } else {
                // now meet next data_id, update next,prev
                now = next->first + next->second.length();
                prev_iter = next;
                ++next;
            }
        }
    } else {
        // 无overlap, data落于un_assembled区间
        now = index;
        // find first key iter bigger than index
        auto prev_iter = _datas.upper_bound(index);
        if (prev_iter != _datas.begin()) {
            --prev_iter;
        }
        // 在un_assembled中, 距离now最近的前一个iter, 其不存在或不与data相交,
        // 则需要data[0]自己注册进_datas充当prev
        if (prev_iter == _datas.end() || prev_iter->first > now ||
            (prev_iter->first + prev_iter->second.length()) < now) {
            _datas[now] = data[0];
            --_remain_capacity;
            prev_iter = _datas.find(now);
            ++now;
        }
        auto next = prev_iter;
        ++next;
        now = prev_iter->first + prev_iter->second.length();
        while (now < index + data.length() && _remain_capacity > 0) {
            if (next == _datas.end()) {
                // no next iter
                prev_iter->second.push_back(data[now - index]);
                --_remain_capacity;
                ++now;
            } else if (now < next->first) {
                // between prev and next
                prev_iter->second.push_back(data[now - index]);
                --_remain_capacity;
                ++now;
            } else {
                // now meet next data_id, update next,prev
                now = next->first + next->second.length();
                prev_iter = next;
                ++next;
            }
        }
    }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    flush_assembled();
    __push_substring(data, index);
    merge_all();

    // move assembled pointer
    while (!_datas.empty() && _datas.begin()->first <= next_unassembled) {
        // move next_unassembled pointer
        next_unassembled = _datas.begin()->first + _datas.begin()->second.length();
        // push to _assembled_strs
        _assembled_strs[_datas.begin()->first] = _datas.begin()->second;
        // remove from _datas that indicate unassembled strs
        _datas.erase(_datas.begin());
    }

    flush_assembled();

    // write eof flag
    if (eof) {
        _endptr = index + data.length();
    }
    // end pointer is start_id of _assembled_strs or in output strs
    if (_endptr <= next_unassembled && (_assembled_strs.empty() || _assembled_strs.begin()->first == _endptr))
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t unassembledBytes = 0;
    for (auto iter : _datas) {
        unassembledBytes += iter.second.length();
    }
    return unassembledBytes;
}

size_t StreamReassembler::assembled_bytes() const {
    size_t assembledBytes = 0;
    for (auto iter : _assembled_strs) {
        assembledBytes += iter.second.length();
    }
    return assembledBytes;
}

bool StreamReassembler::empty() const { return _datas.empty(); }
