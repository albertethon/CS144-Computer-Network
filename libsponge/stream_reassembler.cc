#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

/**
 * @brief 将assembled内字符写进output内
 *
 */
void StreamReassembler::flush_assembled() {
    // 当output有空余且assembled存在字符
    if (_output.remaining_capacity() > 0 && _assembled_strs.size() > 0) {
        const size_t write_bytes = _output.write(_assembled_strs.concatenate());
        // remove from assembled strs
        _assembled_strs.remove_prefix(write_bytes);
    }
}

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity)
    , _capacity(capacity)
    , _endptr(-1)
    , _unassembled_datas()
    , _assembled_strs()
    , next_unassembled(0) {}

void StreamReassembler::__push_substring(const string &data, const size_t index) {
    // overlap的部分assembled, 或空的情况跳过，或index大于当前可接受的最后插入点
    if (data.empty() || index + data.length() <= next_unassembled || index >= _output.bytes_written() + _capacity)
        return;

    const size_t _window_end = get_first_assembled() + _capacity;
    size_t &&index_end = index + data.size();
    // 直接将字符串根据capacity截断, append进assembled里
    const size_t &_data_right_index = min(index_end, _window_end);

    /**
     * @brief overlap部分横跨assembled, un_assembled
        index    _next_assembled_idx   index+data.size()
        V               V                   V
        --+-----------------------------------+-----------------------------
        |///////////////|///////////////////|
        --+-----------------------------------+-----------------------------
     *
     */
    if (index <= next_unassembled) {
        const size_t &_data_write_len = _data_right_index - next_unassembled;
        // string _append_str = string().assign(data.substr(next_unassembled - index, _data_write_len));//right
        // string& _append_str = string().assign(data.substr(next_unassembled - index, _data_write_len));//error
        // string&& _append_str = move(string().assign(data.substr(next_unassembled - index, _data_write_len)));//error

        _assembled_strs.append(
            Buffer(std::move(string().assign(data.substr(next_unassembled - index, _data_write_len)))));

        /**
         * @brief 移除被 data 覆盖的所有str , 覆盖的两部分用两条线来表示
                                first_remove
                                V
            ----------+------------+------------------+---+-----------------...
                    |           |+++| |+++| |++++|
            ----------+------------+------------------+---+-----------------...
            ----------+------------+------------------+---+-----------------...
                    |\\\\\\\\....................\\\\\\\\|
            ----------+------------+------------------+---+-----------------...
                    ^        ^                            ^
                    index   _next_assembled            _data_right_index
         *
         */
        // find first key iter bigger than now
        auto _last_remove = _unassembled_datas.upper_bound(_data_right_index);
        // unassembled第一个就比最新next_unassembled下标大，则不用操作
        if (_last_remove == _unassembled_datas.begin())
            return;
        else
            --_last_remove;

        // 找到最后一个的可以被assembled全覆盖的iter
        while (_last_remove != _unassembled_datas.begin() &&
               (_last_remove->second.size() + _last_remove->first) > _data_right_index) {
            --_last_remove;
        }
        _last_remove++;  // 因为erase的第二个参数last是指向删除的最后一个元素的下一个地方的
        // 移除被全覆盖的iter
        /**
        * @brief   移除要求
                    1. 存在first_remove
                    2. first_remove_end <= data_right
                    3. prev(last_remove)_end <= dat_right
                    满足条件1，2至少能移除一个字串
        */
        auto _first_remove = _unassembled_datas.upper_bound(index);
        if (_first_remove != _unassembled_datas.end() &&
            (_first_remove->first + _first_remove->second.size()) < _data_right_index)
            _unassembled_datas.erase(_first_remove, _last_remove);

        /**
         * @brief 将相交的unassemble str append 进 assemble中
                    |                               first_remove
                    V                               V
            ----------+------------+------------------+---+-----------------...
                    |                               |++++++++++++|
            ----------+------------+------------------+---+-----------------...
            ----------+------------+------------------+---+-----------------...
                    |\\\\\\\\....................\\\\\\\\|
            ----------+------------+------------------+---+-----------------...
                    ^        ^                            ^
                    index   _next_assembled            _data_right_index
         *
         */
        // 看第一个unassembled是否相交( 后续的肯定不相交，因为前面已经移除了全覆盖的字串，只剩下相交和相离字串)
        _last_remove = _unassembled_datas.begin();

        // unassembled为空则跳过
        if (_last_remove == _unassembled_datas.end())
            return;

        if (_last_remove->first <= _data_right_index) {
            _last_remove->second.remove_prefix(_data_right_index - _last_remove->first);
            _assembled_strs.append(std::move(_last_remove->second));
            _unassembled_datas.erase(_last_remove);
        }
    } else {
        /**
         * @brief 无overlap, data落于un_assembled区间
                            prev    first_remove            last
                            V       V                       V        V
            ----------+------------+------------------+---+-----------------...
                    |       |+++++| |+++| |++++|            |++++++| |++|
            ----------+------------+------------------+---+-----------------...
            ----------+------------+------------------+---+-----------------...
                    |           |\\\\\\.................\\\\\\\\|
            ----------+------------+------------------+---+-----------------...
                    ^           ^                               ^
                    assembled   index                           _data_right_index
         *
         */

        /**
         * @brief 移除被 data 覆盖的所有str , 覆盖的两部分用两条线来表示
            _next_assembled_idx  first_remove
                    V            V
            ----------+------------+------------------+---+-----------------...
                    |            |+++| |+++| |++++|
            ----------+------------+------------------+---+-----------------...
            ----------+------------+------------------+---+-----------------...
                    |       |\\\\\\\\\\\\\\\\\...\\\\\\\\|
            ----------+------------+------------------+---+-----------------...
                    ^       ^                            ^
                assembled   index                       _data_right_index
         *
         */
        // 找到index后 第一个字串
        auto &&_first_remove = _unassembled_datas.upper_bound(index);
        // 找到index_end后 第一个字串
        auto &&_last_remove = _unassembled_datas.upper_bound(_data_right_index);

        /**
         * @brief 移除可能被data 完全覆盖的中间字串，需要满足以下要求
                    1. 存在first_remove
                    2. first_remove_end <= data_right
                    3. prev(last_remove)_end <= dat_right
                    满足条件1，2至少能移除一个字串
         *
         */
        if (_first_remove != _unassembled_datas.end() &&
            _first_remove->first + _first_remove->second.size() <= _data_right_index) {
            // 找到被覆盖的最后一个str
            --_last_remove;
            while (_last_remove->first + _last_remove->second.size() > _data_right_index) {
                --_last_remove;
            }
            _unassembled_datas.erase(_first_remove, next(_last_remove));
        }
        // ----------------------------------------------------------------------------//

        // 当unassembled 为空则直接插入并退出
        if (_unassembled_datas.empty()) {
            _unassembled_datas[index] = data.substr(0, min(data.size(), _data_right_index - index));
            return;
        }

        /**
         * @brief 无overlap, data落于un_assembled区间
                            header                          last
                            V                               V        V
            ----------+------------+------------------+---+-----------------...
                    |       |+++++|                         |++++++| |++|
            ----------+------------+------------------+---+-----------------...
            ----------+------------+------------------+---+-----------------...
                    |           |\\\\\\.................\\\\\\\\|
            ----------+------------+------------------+---+-----------------...
                    ^           ^                               ^
                    assembled   index                           _data_right_index
         *
         */
        // find first iter_id > index
        // header_iter 是第一个和data相交的部分
        auto &&header_iter = _unassembled_datas.upper_bound(index);

        if (header_iter == _unassembled_datas.begin()) {
            // begin 就大于index， 则插入index
            _unassembled_datas[index] = data.substr(0, min(data.size(), _data_right_index - index));

            header_iter = _unassembled_datas.begin();
            // 如果 begin 相交于 data尾部， 后续处理
        } else {
            --header_iter;
            /**
             * @brief 分为两部分
                1. index 相交于prev_iter中
                2. index 相离于prev_iter后
                            1                                last
                            V                                V        V
            ----------+------------+------------------+---+-----------------...
                    |       |+++++++|                        |++++++| |++|
            ----------+------------+------------------+---+-----------------...
            ----------+------------+------------------+---+-----------------...
                    |   |+++|   |\\\\\\.................\\\\\\\\|
            ----------+------------+------------------+---+-----------------...
                        ^       ^                               ^
                        2       index                          _data_right_index
             */

            if (index <= (header_iter->first + header_iter->second.size())) {  // 1
                const size_t pos = header_iter->first + header_iter->second.size() - index;
                const size_t str_size = _data_right_index - (pos + index);
                if (pos > data.size())
                    return;  // 当data小于当前的header str时，表示被完全覆盖, 是重复的串
                header_iter->second.append(data.substr(pos, str_size));
            } else {  // 2
                _unassembled_datas.insert_or_assign(index, string(data));
                header_iter = next(header_iter);
            }
        }
        /**
         * @brief 此时已经插入index(或合并进header里), 现处理尾部
                            |                                       last
                            |                                       V        V
                    ----------+------------+------------------+---+-----------------...
                            |                                       |++++++| |++|
                    ----------+------------+------------------+---+-----------------...
                    ----------+------------+------------------+---+-----------------...
                            |           |\\\\\\.................\\\\\\\\|
                    ----------+------------+------------------+---+-----------------...
                            |           ^                               ^
                            |           header                          _data_right_index
                     */
        // 处理相交尾部
        auto overlap_tail = next(header_iter);  // 中间覆盖的部分已经remove了，只剩相交和相离部分
        if (overlap_tail == _unassembled_datas.end()) {
            return;
        }

        const size_t overlap_id = overlap_tail->first;
        auto &overlap_str = overlap_tail->second;

        // 相交才会append, erase
        if (overlap_id <= _data_right_index && (overlap_id + overlap_str.size()) > _data_right_index) {
            overlap_str.remove_prefix(_data_right_index - overlap_id);
            header_iter->second.append(overlap_str);
            _unassembled_datas.erase(overlap_tail);
        }
    }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    flush_assembled();
    __push_substring(data, index);

    flush_assembled();
    next_unassembled = _output.bytes_written() + assembled_bytes();
    // write eof flag
    if (eof) {
        _endptr = index + data.length();
    }
    // end pointer is start_id of _assembled_strs or in output strs
    if (_endptr <= next_unassembled && (_assembled_strs.size() == 0 || get_first_assembled() == _endptr))
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t unassembledBytes = 0;
    for (auto iter : _unassembled_datas) {
        unassembledBytes += iter.second.size();
    }
    return unassembledBytes;
}

size_t StreamReassembler::assembled_bytes() const { return _assembled_strs.size(); }

bool StreamReassembler::empty() const { return _unassembled_datas.empty(); }
