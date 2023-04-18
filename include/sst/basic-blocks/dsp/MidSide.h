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

#ifndef SST_BASICBLOCKS_DSP_MIDSIDE_H
#define SST_BASICBLOCKS_DSP_MIDSIDE_H

namespace sst::basic_blocks::dsp
{
template<size_t blocksize>
void encodeMS(float *__restrict L, float *__restrict R,
              float *__restrict M, float *__restrict S)
{
    for (auto i = 0U; i < blocksize; ++i)
    {
        M[i] = 0.5f * (L[i] + R[i]);
        S[i] = 0.5f * (L[i] - R[i]);
    }
}

template<size_t blocksize>
void decodeMS(float *__restrict M, float *__restrict S,
              float *__restrict L, float *__restrict R)
{
    for (auto i = 0U; i < blocksize; ++i)
    {
        L[i] = M[i] + S[i];
        R[i] = M[i] - S[i];
    }
}
}
#endif // SURGE_MIDSIDE_H
