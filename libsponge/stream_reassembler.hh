#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <map>
#include <string>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
using namespace std;
class StreamReassembler {
  private:
    // Your code here -- add private members as necessary.
    ByteStream _output;      //!< The reassembled in-order byte stream
    const size_t _capacity;  //!< The maximum number of bytes
    size_t _endptr;
    map<size_t, BufferList> _unassembled_datas;
    BufferList _assembled_strs;
    size_t next_unassembled;
    // output buffer满了, 写不进去, 需要在此排队等候
    void flush_assembled();
    void __push_substring(const string &data, const size_t index);

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const string &data, const uint64_t index, const bool eof);

    size_t get_next_unassembled() const { return next_unassembled + (next_unassembled == _endptr); }
    size_t get_first_assembled() const { return _output.bytes_written(); }
    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;
    size_t assembled_bytes() const;
    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
