#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // Set syn, if flagged
    if (seg.header().syn) {
        _syn_set = true;
        _isn = seg.header().seqno;
        // When syn is set, the seqno begin with SYN, so need treat specially
        _reassembler.push_substring(seg.payload().copy(), 0, seg.header().fin);
        return;
    }

    // If syn is not set, just ignore whole segment
    if (!_syn_set)
        return;

    // Calculate index unwrap checkpoint
    uint64_t checkpoint = _reassembler.stream_out().bytes_read() + _reassembler.stream_out().buffer_size() + 1;
    // Calculate index where the segment start
    uint64_t stream_index = unwrap(seg.header().seqno, _isn, checkpoint) - 1;
    _reassembler.push_substring(seg.payload().copy(), stream_index, seg.header().fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    uint64_t abs_seq = _reassembler.stream_out().bytes_read() + _reassembler.stream_out().buffer_size() + 1;
    // When input ended, seqno need to take of FIN
    return _syn_set ? _reassembler.stream_out().input_ended() ? optional(wrap(abs_seq + 1, _isn))
                                                              : optional(wrap(abs_seq, _isn))
                    : nullopt;
}

size_t TCPReceiver::window_size() const {
    return _syn_set ? _capacity - _reassembler.stream_out().buffer_size() : _capacity;
}
