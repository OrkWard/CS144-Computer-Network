#include "wrapping_integers.hh"

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) { return WrappingInt32{static_cast<uint32_t>(n) + isn.raw_value()}; }

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // Calculate wrapped absolute n
    uint32_t wrap_abs_n = n.raw_value() - isn.raw_value();
    // This is one of the candidates. The distance between this and checkpoint
    // must be less than 2^32.
    uint64_t unwarp_n = (checkpoint & 0xFFFFFFFF00000000u) + static_cast<uint64_t>(wrap_abs_n);

    if (unwarp_n > checkpoint && unwarp_n - checkpoint > 0x80000000u && unwarp_n > (1ul << 32))
        // too large
        return unwarp_n - (1ul << 32);
    else if (unwarp_n < checkpoint && checkpoint - unwarp_n > 0x80000000u && unwarp_n + (1ul << 32) < (0ul - 1))
        // too small
        return unwarp_n + (1ul << 32);
    else
        return unwarp_n;
}
