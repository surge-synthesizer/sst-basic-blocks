/*
 * sst-basic-blocks - a Surge Synth Team product
 *
 * Provides basic tools and blocks for use on the audio thread in
 * synthesis, including basic DSP and modulation functions
 *
 * Copyright 2019 - 2023, Various authors, as described in the github
 * transaction log.
 *
 * sst-basic-blocks is released under the Gnu General Public Licence
 * V3 or later (GPL-3.0-or-later). The license is found in the file
 * "LICENSE" in the root of this repository or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * All source for sst-basic-blocks is available at
 * https://github.com/surge-synthesizer/sst-basic-blocks
 */
#ifndef SST_BASIC_BLOCKS_MECHANICS_ENDIAN_OPS_H
#define SST_BASIC_BLOCKS_MECHANICS_ENDIAN_OPS_H

namespace sst::basic_blocks::mechanics
{

// TODO - one day we will have different endiand will need an ifdef here
#define DO_SWAP 0

inline uint32_t swap_endian(uint32_t t)
{
    return ((t << 24) & 0xff000000) | ((t << 8) & 0x00ff0000) | ((t >> 8) & 0x0000ff00) |
           ((t >> 24) & 0x000000ff);
}

inline int endian_write_int32LE(uint32_t t)
{
#if DO_SWAP
    return swap_endian(t);
#else
    return t;
#endif
}
inline auto endian_read_int32LE(uint32_t a) { return endian_write_int32LE(a); }

inline float endian_write_float32LE(float f)
{
#if DO_SWAP
    int t = *((int *)&f);
    t = swap_endian(t);
    return *((float *)&t);
#else
    return f;
#endif
}

inline int endian_write_int32BE(uint32_t t)
{
#if !DO_SWAP
    return swap_endian(t);
#else
    return t;
#endif
}

inline auto endian_read_int32BE(uint32_t a) { return endian_write_int32BE(a); }

inline short endian_write_int16LE(uint16_t t)
{
#if DO_SWAP
    return ((t << 8) & 0xff00) | ((t >> 8) & 0x00ff);
#else
    return t;
#endif
}

inline auto endian_read_int16LE(uint16_t a) { return endian_write_int16LE(a); }

inline uint16_t endian_write_int16BE(uint16_t t)
{
#if !DO_SWAP
    return ((t << 8) & 0xff00) | ((t >> 8) & 0x00ff);
#else
    return t;
#endif
}

inline auto endian_read_int16BE(uint16_t a) { return endian_write_int16BE(a); }

} // namespace sst::basic_blocks::mechanics

#endif // SHORTCIRCUIT_ENDIAN_OPS_H
