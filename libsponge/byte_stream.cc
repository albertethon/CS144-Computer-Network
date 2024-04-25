#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : buffer(), read_bytes(0), written_bytes(0), maximum(capacity), ended(false) {}

size_t ByteStream::write(const string &data) {
    if (ended)
        return 0;
    const size_t write_size = min(maximum - buffer_size(), data.size());
    buffer.append(BufferList(std::move(string().assign(data.begin(), data.begin() + write_size))));
    written_bytes += write_size;
    return write_size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    const size_t peek_size = min(len, buffer_size());
    return buffer.concatenate().substr(0, peek_size);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    const size_t pop_size = min(len, buffer_size());
    buffer.remove_prefix(pop_size);
    read_bytes += pop_size;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    const size_t read_size = min(len, buffer_size());
    string retval(peek_output(read_size));
    pop_output(read_size);

    return retval;
}

void ByteStream::end_input() { ended = true; }

bool ByteStream::input_ended() const {
    // had written eof flag
    return ended;
}

size_t ByteStream::buffer_size() const { return buffer.size(); }

bool ByteStream::buffer_empty() const { return buffer.size() == 0; }

bool ByteStream::eof() const { return ended && buffer.size() == 0; }

size_t ByteStream::bytes_written() const { return written_bytes; }

size_t ByteStream::bytes_read() const { return read_bytes; }

size_t ByteStream::remaining_capacity() const {
    // remain space that can write
    return maximum - buffer.size();
}
