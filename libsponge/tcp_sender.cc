#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <limits>
#include <random>

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _rto(retx_timeout)
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const {
    auto last_seg = _segments_outstanding.back();
    auto last_seqno = unwrap(last_seg.header().seqno, _isn, _next_seqno);
    return last_seqno + last_seg.length_in_sequence_space() - _next_seqno;
}

void TCPSender::fill_window() {
    uint16_t bytes_to_send =
        min({_window_size, static_cast<uint16_t>(TCPConfig::MAX_PAYLOAD_SIZE), static_cast<uint16_t>(1)});
    TCPSegment new_segment{};
    new_segment.header().seqno = WrappingInt32{wrap(_next_seqno, _isn)};

    // first tcpsegment
    if (_next_seqno == 0)
        new_segment.header().syn = true;

    new_segment.payload() = Buffer{_stream.read(bytes_to_send)};
    // stream eof
    if (_stream.eof())
        new_segment.header().fin = true;

    _segments_out.push(new_segment);
    _segments_outstanding.push_back(new_segment);

    // start timer if stopped
    if (_timer.state == TimerState::Stop) {
        _timer.state = TimerState::Running;
        _timer.start_time = _time;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    // update status
    _window_size = window_size;
    // if no new segment acknowledged
    if (_next_seqno >= unwrap(ackno, _isn, _next_seqno)) {
        return;
    }
    _next_seqno = unwrap(ackno, _isn, _next_seqno);

    // reset rto and timer
    _rto = _initial_retransmission_timeout;
    _timer.start_time = _time;
    _retransmission_count = 0;

    // clear fully acknowledged outstanding segments
    while (1) {
        auto segment = _segments_outstanding.front();
        auto seg_seqno = unwrap(segment.header().seqno, _isn, _next_seqno);
        if (seg_seqno + segment.length_in_sequence_space() > _next_seqno) {
            break;
        }
        _segments_outstanding.pop_front();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _time += ms_since_last_tick;

    // expired
    if (_timer.start_time + _rto <= _time) {
        // resend earliest segment
        _segments_out.push(_segments_outstanding.front());

        // double the rto and increment count, if window is nonzero
        if (_window_size != 0) {
            _rto *= 2;
            _retransmission_count += 1;
        }

        // restart timer
        _timer.start_time = _time;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _retransmission_count; }

void TCPSender::send_empty_segment() {
    TCPSegment new_segment{};
    new_segment.header().seqno = WrappingInt32{wrap(_next_seqno, _isn)};

    _segments_out.push(new_segment);
}
