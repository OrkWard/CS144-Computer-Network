#include "tcp_connection.hh"

#include <iostream>

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time - _last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    // record time
    _last_segment_received = _time;

    // reset received, end connection
    if (seg.header().rst) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        return;
    }

    if (seg.header().fin && !_sender.stream_in().eof())
        _linger_after_streams_finish = false;

    // give it to receiver
    _receiver.segment_received(seg);

    // give it to sender if ack set
    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    // send segment if coming segment occupy at least one seqno
    if (seg.length_in_sequence_space() > 0) {
        _sender.fill_window();
        // nothing been sent, send an empty segment
        if (_sender.segments_out().empty())
            _sender.send_empty_segment();

        push_segments_out();
    }
}

bool TCPConnection::active() const {
    if (_sender.stream_in().error() || _receiver.stream_out().error())
        return false;
    if (_linger_after_streams_finish)
        return _time - _last_segment_received < 10 * _cfg.rt_timeout;
    return _sender.bytes_in_flight() > 0 || !_sender.stream_in().eof();
}

size_t TCPConnection::write(const string &data) {
    const size_t bytes_written = _sender.stream_in().write(data);
    _sender.fill_window();
    push_segments_out();
    return bytes_written;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    push_segments_out();
    // reset connection, if retransmition
    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS && _linger_after_streams_finish) {
        // set both stream to error
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();

        send_reset();
    }

    // end connection, if time elapsed since last receive exceed limit
    if (time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
        send_reset();
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    push_segments_out();
}

void TCPConnection::connect() {
    _sender.fill_window();
    push_segments_out();
}

void TCPConnection::push_segments_out() {
    while (!_sender.segments_out().empty()) {
        TCPSegment new_seg = _sender.segments_out().front();
        _sender.segments_out().pop();

        // fill in the ack if possible
        if (_receiver.ackno().has_value()) {
            new_seg.header().ack = true;
            new_seg.header().ackno = _receiver.ackno().value();
            new_seg.header().win = _receiver.window_size() > std::numeric_limits<uint16_t>::max()
                                       ? std::numeric_limits<uint16_t>::max()
                                       : _receiver.window_size();
        }

        _segments_out.push(new_seg);
    }
}

void TCPConnection::send_reset() {
    _sender.send_empty_segment();
    TCPSegment new_seg = _sender.segments_out().front();
    _sender.segments_out().pop();
    new_seg.header().rst = true;
    _segments_out.push(new_seg);
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            send_reset();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
