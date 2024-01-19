#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity){};

bool StreamReassembler::range_overlap(const std::pair<uint64_t, uint64_t> &r1,
                                      const std::pair<uint64_t, uint64_t> &r2) {
    return (r1.first <= r2.second && r2.second <= r1.second) || (r1.first <= r2.first && r2.first <= r1.second);
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // range overflow, just ignore
    if ((index + data.length()) > _capacity + _cap_start) {
        return;
    }

    auto new_datagram = std::make_unique<datagram>(data, std::pair<uint64_t, uint64_t>{index, index});

    auto overlapped_datagrams = std::vector<std::shared_ptr<datagram>>{};

    for (const auto &datagram : _datagrams)
        if (range_overlap(datagram->range, new_datagram->range)) {
            overlapped_datagrams.emplace_back(datagram);
        }

    std::unique_ptr<datagram> merged_datagram = std::move(new_datagram);
    for (const auto &datagram : overlapped_datagrams) {
        merged_datagram = merge_datagram(*datagram, *merged_datagram);
        static_cast<void>(std::remove(_datagrams.begin(), _datagrams.end(), datagram));
    }

    _datagrams.push_back(std::make_shared<datagram>(std::move(merged_datagram)));
}

size_t StreamReassembler::unassembled_bytes() const { return {}; }

bool StreamReassembler::empty() const { return {}; }
