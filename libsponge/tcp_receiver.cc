#include "tcp_receiver.hh"

#include "wrapping_integers.hh"

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // Set syn, if flagged
    if (seg.header().syn) {
        _syn_set = true;
        _isn = seg.header().seqno;
    }

    // If syn is not set, just ignore whole segment
    if (!_syn_set)
        return;

    // Calculate index unwrap checkpooint
    uint64_t checkpoint = _reassembler.stream_out().bytes_read() + _reassembler.stream_out().buffer_size() + 1;
    // Calculate index where the segment start
    uint64_t stream_index = unwrap(seg.header().seqno, _isn, checkpoint) - 1;
    _reassembler.push_substring(seg.payload().copy(), stream_index, seg.header().fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    uint64_t abs_seq = _reassembler.stream_out().bytes_read() + _reassembler.stream_out().buffer_size() + 1;
    return _syn_set ? optional<WrappingInt32>(wrap(abs_seq, _isn)) : nullopt;
}

size_t TCPReceiver::window_size() const {
    return _syn_set ? _capacity - _reassembler.stream_out().buffer_size() : _capacity;
}
