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
#ifndef SST_BASIC_BLOCKS_MECHANICS_BLOCK_OPS_H
#define SST_BASIC_BLOCKS_MECHANICS_BLOCK_OPS_H

namespace sst::basic_blocks::mechanics
{
// TODO: Look at these in godbolt to see how well they optimize for 8
// 16 and 32 and if we should simdize by hand or not. (probably not)
template <size_t blocksize> inline void accumulate_from_to(const float *src, float *dst)
{
    for (auto i = 0U; i < blocksize; ++i)
        dst[i] += src[i];
}

template <size_t blocksize> inline void copy_from_to(const float *src, float *dst)
{
    for (auto i = 0U; i < blocksize; ++i)
        dst[i] = src[i];
}

template <size_t blockSize> inline void scale_by(const float *scale, float *target)
{
    for (auto i=0U; i<blockSize; ++i)
        target[i] *= scale[i];
}

template <size_t blockSize> inline void scale_by(const float *scale, float *targetL, float *targetR)
{
    for (auto i=0U; i<blockSize; ++i)
    {
        targetL[i] *= scale[i];
        targetR[i] *= scale[i];
    }
}
} // namespace sst::basic_blocks::block_ops

#endif // SHORTCIRCUIT_BLOCK_OPS_H
