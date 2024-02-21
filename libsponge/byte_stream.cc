#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : buffer(), read_bytes(0), write_bytes(0), maximum(capacity), ended(false) {}

size_t ByteStream::write(const string &data) {
    size_t len = data.size();
    size_t i;
    for (i = 0; i < len && !ended && buffer.size() < maximum; ++i) {
        buffer.push_back(data[i]);
    }
    write_bytes += i;
    return i;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    if (this->buffer_empty()) {
        return "";
    } else {
        string retval;
        for (size_t i = 0; i < len && !eof(); ++i) {
            retval.push_back(buffer.at(i));
        }
        return retval;
    }
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (!this->buffer_empty()) {
        size_t i;
        for (i = 0; i < len && !eof(); ++i) {
            buffer.pop_front();
        }
        read_bytes += i;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string retval(move(peek_output(len)));
    pop_output(len);

    return retval;
}

void ByteStream::end_input() {
    write("\0");
    ended = true;
}

bool ByteStream::input_ended() const {
    // had written eof flag
    return ended;
}

size_t ByteStream::buffer_size() const { return buffer.size(); }

bool ByteStream::buffer_empty() const { return buffer.empty(); }

bool ByteStream::eof() const { return ended && buffer.empty(); }

size_t ByteStream::bytes_written() const { return write_bytes; }

size_t ByteStream::bytes_read() const { return read_bytes; }

size_t ByteStream::remaining_capacity() const {
    // remain space that can write
    return maximum - buffer.size();
}
