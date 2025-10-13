/*
* sst-basic-blocks - an open source library of core audio utilities
 * built by Surge Synth Team.
 *
 * Provides a collection of tools useful on the audio thread for blocks,
 * modulation, etc... or useful for adapting code to multiple environments.
 *
 * Copyright 2023, various authors, as described in the GitHub
 * transaction log. Parts of this code are derived from similar
 * functions original in Surge or ShortCircuit.
 *
 * sst-basic-blocks is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html.
 *
 * A very small number of explicitly chosen header files can also be
 * used in an MIT/BSD context. Please see the README.md file in this
 * repo or the comments in the individual files. Only headers with an
 * explicit mention that they are dual licensed may be copied and reused
 * outside the GPL3 terms.
 *
 * All source in sst-basic-blocks available at
 * https://github.com/surge-synthesizer/sst-basic-blocks
 */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>

#include "sst/basic-blocks/dsp/EllipticBlepOscillators.h"

struct RIFFWavWriter
{
    static constexpr size_t fnameLen{8192};
    char fname[fnameLen];
    FILE *outf{nullptr};
    size_t elementsWritten{0};
    size_t fileSizeLocation{0};
    size_t dataSizeLocation{0};
    size_t dataLen{0};

    uint16_t nChannels{2};

    std::string errMsg{};

    RIFFWavWriter() {}

    RIFFWavWriter(const char *f, uint16_t chan) : nChannels(chan)
    {
        strncpy(fname, f, fnameLen);
        fname[fnameLen - 1] = '\0';
    }
    ~RIFFWavWriter()
    {
        if (!closeFile())
        {
            // Unhandleable error here. Throwing is bad. Reporting is useless.
        }
    }

    void writeRIFFHeader()
    {
        pushc4('R', 'I', 'F', 'F');
        fileSizeLocation = elementsWritten;
        pushi32(0);
        pushc4('W', 'A', 'V', 'E');
    }

    void writeFMTChunk(int32_t samplerate)
    {
        pushc4('f', 'm', 't', ' ');
        pushi32(16);
        pushi16(3);         // IEEE float
        pushi16(nChannels); // channels
        pushi32(samplerate);
        pushi32(samplerate * nChannels * 4); // channels * bytes * samplerate
        pushi16(nChannels * 4);              // align on pair of 4 byte samples
        pushi16(8 * 4);                      // bits per sample
    }

    void writeINSTChunk(char keyroot, char keylow, char keyhigh, char vellow, char velhigh)
    {
        pushc4('i', 'n', 's', 't');
        pushi32(8);
        pushi8(keyroot);
        pushi8(0);
        pushi8(127);
        pushi8(keylow);
        pushi8(keyhigh);
        pushi8(vellow);
        pushi8(velhigh);
        pushi8(0);
    }

    void startDataChunk()
    {
        pushc4('d', 'a', 't', 'a');
        dataSizeLocation = elementsWritten;
        pushi32(0);
    }

    void pushSamples(float d[2])
    {
        if (outf)
        {
            elementsWritten += fwrite(d, 1, nChannels * sizeof(float), outf);
            dataLen += nChannels * sizeof(float);
        }
    }

    void pushInterleavedBlock(float *d, size_t nSamples)
    {
        if (outf)
        {
            elementsWritten += fwrite(d, 1, nSamples * sizeof(float), outf);
            dataLen += nSamples * sizeof(float);
        }
    }

    void pushc4(char a, char b, char c, char d)
    {
        char f[4]{a, b, c, d};
        pushc4(f);
    }
    void pushc4(char f[4])
    {
        if (outf)
            elementsWritten += fwrite(f, sizeof(char), 4, outf);
    }

    void pushi32(int32_t i)
    {
        if (outf)
            elementsWritten += std::fwrite(&i, sizeof(char), sizeof(uint32_t), outf);
    }

    void pushi16(int16_t i)
    {
        if (outf)
            elementsWritten += fwrite(&i, sizeof(char), sizeof(uint16_t), outf);
    }

    void pushi8(char i)
    {
        if (outf)
            elementsWritten += std::fwrite(&i, 1, 1, outf);
    }
    [[nodiscard]] bool openFile()
    {
        elementsWritten = 0;
        dataLen = 0;
        dataSizeLocation = 0;
        fileSizeLocation = 0;

        outf = fopen(fname, "wb");
        if (!outf)
        {
            errMsg = "Unable to open '" + std::string(fname) + "' for writing";
            return false;
        }
        return true;
    }

    bool isOpen() { return outf != nullptr; }
    [[nodiscard]] bool closeFile()
    {
        if (outf)
        {
            int res;
            res = std::fseek(outf, fileSizeLocation, SEEK_SET);
            if (res)
            {
                std::cout << "SEEK ZERO ERROR" << std::endl;
                return false;
            }
            int32_t chunklen = elementsWritten - 8; // minus riff and size
            fwrite(&chunklen, sizeof(uint32_t), 1, outf);

            res = std::fseek(outf, dataSizeLocation, SEEK_SET);
            if (res)
            {
                return false;
                std::cout << "SEEK ONE ERROR" << std::endl;
            }
            fwrite(&dataLen, sizeof(uint32_t), 1, outf);
            std::fclose(outf);
            outf = nullptr;
        }
        return true;
    }

    [[nodiscard]] size_t getSampleCount() const { return dataLen / (nChannels * sizeof(float)); }
};

template<typename T>
int go(const char *fname)
{
    double sr{48000};
    T inst;
    inst.setSampleRate(sr);

    std::string outPath{fname};
    RIFFWavWriter writer(outPath.c_str(), 1);
    if (!writer.openFile())
        return 2;
    writer.writeRIFFHeader();
    writer.writeFMTChunk(44100);

    writer.startDataChunk();

    for (int i=0; i<sr* 2; ++i)
    {
        if (i % 8 == 0)
        {
            float fi = i / sr;
            if (fi > 1)
                fi = 1 - (fi-1);
            inst.setFrequency(100 + fi * (sr - 100) * 0.25);
        }
        float d[2];
        float s = inst.step();
        d[0] = s;
        d[1] = s;
        writer.pushSamples(d);
    }
    if (!writer.closeFile())
        return 1;

    return 0;
}

int main()
{
    static constexpr int blockSize{8};
    namespace sdsp = sst::basic_blocks::dsp;
    using ss = sdsp::BlockInterpSmoothingStrategy<blockSize>;
    go<sdsp::EBSaw<ss>>( "/tmp/sweepSaw.wav");
    go<sdsp::EBTri<ss>>( "/tmp/sweepTri.wav");
    go<sdsp::EBPulse<ss>>( "/tmp/sweepPulse.wav");
    go<sdsp::EBApproxSin<ss>>( "/tmp/sweepSin.wav");
    go<sdsp::EBApproxSemiSin<ss>>( "/tmp/sweepSemiSin.wav");

    return 0;
}