#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t capacity) { _buffer.resize(capacity); }

size_t ByteStream::write(const string &data) {
    unsigned int i = 0;
    for (; buffer_size() + i < _buffer.size() && i < data.size(); ++i) {
        _buffer[(_end + i) % _buffer.size()] = data[i];
    }

    _end = (_end + i) % _buffer.size();
    _write_count += i;
    _size += i;

    return i;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string output{};
    for (unsigned int i = 0; i < min(len, buffer_size()); ++i) {
        output += _buffer[(_start + i) % _buffer.size()];
    }
    return output;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    _start = (_start + min(len, _size)) % _buffer.size();
    _read_count += min(len, _size);
    _size -= min(len, _size);
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string output = peek_output(len);
    pop_output(len);
    return output;
}

void ByteStream::end_input() { _input_end = true; }

bool ByteStream::input_ended() const { return _input_end; }

size_t ByteStream::buffer_size() const { return _size; }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return buffer_empty() && _input_end; }

size_t ByteStream::bytes_written() const { return _write_count; }

size_t ByteStream::bytes_read() const { return _read_count; }

size_t ByteStream::remaining_capacity() const { return _buffer.size() - buffer_size(); }
