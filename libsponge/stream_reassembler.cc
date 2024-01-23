#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity){};

bool StreamReassembler::range_overlap(const std::pair<uint64_t, uint64_t> &r1,
                                      const std::pair<uint64_t, uint64_t> &r2) {
    return !(r1.second < r2.first || r1.first > r2.second);
}
std::unique_ptr<StreamReassembler::datagram> StreamReassembler::merge_datagram(const datagram &d1, const datagram &d2) {
    const datagram &lower = d1.range.first <= d2.range.first ? d1 : d2;
    const datagram &larger = d1.range.second >= d2.range.second ? d1 : d2;

    auto data = lower.data;
    if (&lower != &larger)
        data += larger.data.substr(lower.range.second - larger.range.first);

    return make_unique<datagram>(data, std::pair<uint64_t, uint64_t>(lower.range.first, larger.range.second));
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // set eof position
    if (eof) {
        _eof = index + data.length();
    }

    // identify buffer start and end
    auto buffer_start = _output.bytes_read();
    auto buffer_end = buffer_start + _output.buffer_size();

    // range overflow, just ignore
    if (index >= buffer_start + _capacity || index + data.length() <= buffer_end) {
        if (_eof == buffer_start) {
            _output.end_input();
        }
        return;
    }

    // new datagram, throw overflowed
    auto trancated_data = data.substr(0, buffer_start + _capacity - index);
    auto new_datagram = std::make_unique<datagram>(
        trancated_data, std::pair<uint64_t, uint64_t>{index, index + trancated_data.length()});

    auto overlapped_datagrams = std::vector<std::shared_ptr<datagram>>{};

    for (const auto &datagram : _datagrams)
        if (range_overlap(datagram->range, new_datagram->range)) {
            overlapped_datagrams.emplace_back(datagram);
        }

    std::shared_ptr<datagram> merged_datagram = std::move(new_datagram);
    for (const auto &datagram : overlapped_datagrams) {
        merged_datagram = merge_datagram(*datagram, *merged_datagram);
        // remove merged datagram
        for (auto iter = _datagrams.begin(); iter != _datagrams.end(); ++iter)
            if (*iter == datagram) {
                _datagrams.erase(iter);
                break;
            }
    }

    if (index <= buffer_end) {
        _output.write(merged_datagram->data.substr(max(buffer_end - index, 0ul)));

        // buffer end updated
        if (_eof == buffer_start + _output.buffer_size()) {
            _output.end_input();
        }
        return;
    }

    _datagrams.emplace_back(merged_datagram);
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t count = 0;
    for (const auto &datagram : _datagrams) {
        count += datagram->data.size();
    }
    return count;
}

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }
