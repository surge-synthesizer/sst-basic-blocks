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

/*
 * This class is taken from the Six Sines synthesizer whose
 * code and is found at https://github.com/baconpaul/six-sines
 *
 * The Six Sines source code was released under the MIT license,
 * and this simplified file can be used in an MIT context as well.
 */

#ifndef INCLUDE_SST_BASIC_BLOCKS_TABLES_SIXSINESWAVEPROVIDER_H
#define INCLUDE_SST_BASIC_BLOCKS_TABLES_SIXSINESWAVEPROVIDER_H

#include <cassert>
#include <cstring>
#include <functional>
#include <utility>

#include "configuration.h"
#include <sst/basic-blocks/simd/setup.h>

namespace sst::basic_blocks::tables
{
struct SixSinesWaveProvider
{
    enum WaveForm
    {
        SIN = 0, // these stream so you know....
        SIN_FIFTH,
        SQUARISH,
        SAWISH,
        TRIANGLE,
        SIN_OF_CUBED,

        TX2,
        TX3,
        TX4,
        TX5,
        TX6,
        TX7,
        TX8,

        SPIKY_TX2,
        SPIKY_TX4,
        SPIKY_TX6,
        SPIKY_TX8,

        HANN_WINDOW,
        BLACKMAN_HARRIS_WINDOW,
        HALF_BLACKMAN_HARRIS_WINDOW,
        TUKEY_WINDOW,

        NUM_WAVEFORMS
    };

    static constexpr size_t nPoints{1 << 12}, nQuadrants{4};
    inline static double xTable[nQuadrants][nPoints + 1];
    inline static float quadrantTable[NUM_WAVEFORMS][nQuadrants][nPoints + 1];
    inline static float dQuadrantTable[NUM_WAVEFORMS][nQuadrants][nPoints + 1];

    inline static float cubicHermiteCoefficients[nQuadrants][nPoints];
    inline static float linterpCoefficients[2][nPoints];

    inline static SIMD_M128 simdFullQuad alignas(
        16)[NUM_WAVEFORMS][nQuadrants * nPoints];           // for each quad it is q, q+1, dq + 1
    inline static SIMD_M128 simdCubic alignas(16)[nPoints]; // it is cq, cq+1, cdq, cd1+1
    inline static bool staticsInitialized;
    inline static bool butOnlyForSine;

    SIMD_M128 *simdQuad;

    SixSinesWaveProvider()
    {
        initializeStatics(false);
        simdQuad = simdFullQuad[0];
    }
    SixSinesWaveProvider(bool allwaves)
    {
        initializeStatics(allwaves);
        simdQuad = simdFullQuad[0];
    }

    void setSampleRate(double sr) { frToPhase = (1 << 26) / sr; }
    static void fillTable(int WF, std::function<std::pair<double, double>(double x, int Q)> der)
    {
        static constexpr double dxdPhase = 1.0 / (nQuadrants * (nPoints - 1));
        for (int Q = 0; Q < nQuadrants; ++Q)
        {
            for (int i = 0; i < nPoints + 1; ++i)
            {
                auto [v, dvdx] = der(xTable[Q][i], Q);
                quadrantTable[WF][Q][i] = static_cast<float>(v);
                dQuadrantTable[WF][Q][i] = static_cast<float>(dvdx * dxdPhase);
            }
        }
    }

    static void initializeStatics(bool allWaves)
    {
        if (staticsInitialized)
            return;

        memset(quadrantTable, 0, sizeof(quadrantTable));
        memset(dQuadrantTable, 0, sizeof(dQuadrantTable));

        for (int i = 0; i < nPoints + 1; ++i)
        {
            for (int Q = 0; Q < nQuadrants; ++Q)
            {
                xTable[Q][i] = (1.0 * i / (nPoints - 1) + Q) * 0.25;
            }
        }

        static constexpr double twoPi{2.0 * M_PI};
        // Waveform 0: sin(2pix);
        fillTable(WaveForm::SIN, [](double x, int Q) {
            return std::make_pair(sin(twoPi * x), twoPi * cos(twoPi * x));
        });

        if (allWaves)
        {
            // Waveform 1: sin(2pix)^4. Deriv is 5 2pix sin(2pix)^4 cos(2pix)
            fillTable(WaveForm::SIN_FIFTH, [](double x, int Q) {
                auto s = sin(twoPi * x);
                auto c = cos(twoPi * x);
                auto v = s * s * s * s * s;
                auto dv = 5 * twoPi * s * s * s * s * c;
                return std::make_pair(v, dv);
            });

            // Waveform 2: Square-ish with sin 8 transitions
            fillTable(WaveForm::SQUARISH, [](double x, int Q) {
                static constexpr double winFreq{8.0};
                static constexpr double dFr{1.0 / (4 * winFreq)};
                static constexpr double twoPiF{twoPi * winFreq};
                float v{0}, dv{0};
                if (x <= dFr || x > 1.0 - dFr)
                {
                    v = sin(twoPiF * x);
                    dv = twoPiF * cos(2.0 * M_PI * 4 * x);
                }
                else if (x <= 0.5 - dFr)
                {
                    v = 1.0;
                    dv = 0.0;
                }
                else if (x < 0.5 + dFr)
                {
                    v = -sin(twoPiF * x);
                    dv = twoPiF * cos(2.0 * M_PI * winFreq * x);
                }
                else
                {
                    v = -1.0;
                    dv = 0.0;
                }
                return std::make_pair(v, dv);
            });

            // Waveform 3: Saw-ish with sin 4 transitions
            // ALl in x < 0 < 1
            // a = 1 - 2 x
            // b = sin 6pi x
            // c = sin(pi (32 (x - 0.5)^6 + 0.5))
            // res = b + c * (a-b)
            // dres = db + dc (a-b) + c (da - db)

            static constexpr double sqrFreq{4};
            static constexpr double twoPiS{sqrFreq * twoPi};
            auto osp = 1.0 / (twoPiS);
            auto co = osp * acos(-2 * osp) / (twoPi /* rad->0.1 units */);

            fillTable(WaveForm::SAWISH, [co](double x, int Q) {
                auto a = 1.0 - 2 * x;
                auto da = -2;

                auto b = sin(6 * M_PI * x);
                auto db = 6 * M_PI * cos(6 * M_PI * x);

                auto c = sin(M_PI * (32 * pow((x - 0.5), 6) + 0.5));
                // gross
                auto eps = 0.00001;
                auto cp = sin(M_PI * (32 * pow((x + eps - 0.5), 6) + 0.5));
                auto cm = sin(M_PI * (32 * pow((x - eps - 0.5), 6) + 0.5));
                auto dc = (cp - cm) / 2 * eps;

                auto v = b + c * (a - b);
                auto dv = db + dc * (a - b) + c * (da - db);

                // Convert to upward saw

                return std::make_pair(-v, -dv);
            });

            fillTable(WaveForm::TRIANGLE, [](double x, int Q) {
                if (Q == 0)
                {
                    return std::make_pair(4 * x, 4.0);
                }
                else if (Q == 3)
                {
                    return std::make_pair(4 * x - 4, 4.0);
                }
                else
                {
                    return std::make_pair(2.0 - 4.0 * x, -4.0);
                }
                return std::make_pair(0.0, 0.0);
            });
            fillTable(WaveForm::SIN_OF_CUBED, [](double x, int Q) {
                auto z = x * 2 - 1;
                auto dzdx = 2;
                auto v = sin(twoPi * z * z * z);
                auto dvdz = 3 * twoPi * z * z * cos(twoPi * z * z * z);
                auto dv = dvdz * dzdx;

                // Above is a downward saw and we want upward
                return std::make_pair(v, dv);
            });

            fillTable(SixSinesWaveProvider::TX2, [](double x, int Q) -> std::pair<double, double> {
                auto v = 0.0;
                auto dv = 0.0;
                if (Q == 0 || Q == 1)
                {
                    v = 0.5 * (sin(4.0 * M_PI * (x - 0.125)) + 1);
                    dv = 2.0 * M_PI * cos(4.0 * M_PI * (x - 0.125));
                }
                else
                {
                    v = -0.5 * (sin(4.0 * M_PI * (x - 0.125)) + 1);
                    dv = -2.0 * M_PI * cos(4.0 * M_PI * (x - 0.125));
                }
                return {v, dv};
            });

            fillTable(SixSinesWaveProvider::WaveForm::SPIKY_TX2, [](double x, int Q) {
                double v, dv;
                auto s = sin(twoPi * x);
                auto c = cos(twoPi * x);

                switch (Q)
                {
                case 0:
                    v = 1 - c;
                    dv = twoPi * s;
                    break;
                case 1:
                    v = 1 + c;
                    dv = -twoPi * s;
                    break;
                case 2:
                    v = -1 - c;
                    dv = twoPi * s;
                    break;
                case 3:
                    v = c - 1;
                    dv = -twoPi * s;
                    break;
                }

                return std::make_pair(v, dv);
            });

            fillTable(SixSinesWaveProvider::WaveForm::TX3, [](double x, int Q) {
                double v, dv;
                auto s = sin(twoPi * x);
                auto c = cos(twoPi * x);

                switch (Q)
                {
                case 0:
                case 1:
                    v = s;
                    dv = twoPi * c;
                    break;
                case 2:
                case 3:
                    v = 0;
                    dv = 0;
                    break;
                }

                return std::make_pair(v, dv);
            });

            fillTable(SixSinesWaveProvider::TX4, [](double x, int Q) -> std::pair<double, double> {
                auto v = 0.0;
                auto dv = 0.0;
                if (Q == 0 || Q == 1)
                {
                    v = 0.5 * (sin(4.0 * M_PI * (x - 0.125)) + 1);
                    dv = 2.0 * M_PI * cos(4.0 * M_PI * (x - 0.125));
                }

                return {v, dv};
            });

            fillTable(SixSinesWaveProvider::WaveForm::SPIKY_TX4, [](double x, int Q) {
                double v, dv;
                auto s = sin(twoPi * x);
                auto c = cos(twoPi * x);

                switch (Q)
                {
                case 0:
                    v = 1 - c;
                    dv = twoPi * s;
                    break;
                case 1:
                    v = 1 + c;
                    dv = -twoPi * s;
                    break;
                case 2:
                case 3:
                    v = 0;
                    dv = 0;
                    break;
                }

                return std::make_pair(v, dv);
            });

            fillTable(SixSinesWaveProvider::WaveForm::TX5, [](double x, int Q) {
                double v, dv;
                auto s = sin(2 * twoPi * x);
                auto c = cos(2 * twoPi * x);

                switch (Q)
                {
                case 0:
                case 1:
                    v = s;
                    dv = 2 * twoPi * c;
                    break;
                case 2:
                case 3:
                    v = 0;
                    dv = 0;
                    break;
                }

                return std::make_pair(v, dv);
            });

            fillTable(SixSinesWaveProvider::WaveForm::TX6,
                      [](double x, int Q) -> std::pair<double, double> {
                          auto v = 0.0;
                          auto dv = 0.0;
                          if (Q == 0)
                          {
                              v = 0.5 * (sin(8.0 * M_PI * (x - 0.0625)) + 1);
                              dv = 4.0 * M_PI * cos(8.0 * M_PI * (x - 0.0625));
                          }
                          else if (Q == 1)
                          {
                              v = -0.5 * (sin(8.0 * M_PI * (x - 0.0625)) + 1);
                              dv = -4.0 * M_PI * cos(8.0 * M_PI * (x - 0.0625));
                          }
                          return {v, dv};
                      });
            fillTable(SixSinesWaveProvider::WaveForm::SPIKY_TX6, [](double x, int Q) {
                double v{0}, dv{0};
                auto s = sin(2 * twoPi * x);
                auto c = cos(2 * twoPi * x);

                auto OCT = Q * 2;
                if (x > .125 && Q == 0)
                    OCT++;
                else if (x > .375 && Q == 1)
                    OCT++;

                switch (OCT)
                {
                case 0:
                    v = 1 - c;
                    dv = 2 * twoPi * s;
                    break;
                case 1:
                    v = 1 + c;
                    dv = -2 * twoPi * s;
                    break;
                case 2:
                    v = -1 - c;
                    dv = 2 * twoPi * s;
                    break;
                case 3:
                    v = c - 1;
                    dv = -2 * twoPi * s;
                    break;
                default:
                    break;
                }

                return std::make_pair(v, dv);
            });

            fillTable(SixSinesWaveProvider::WaveForm::TX7, [](double x, int Q) {
                double v, dv;
                auto s = sin(2 * twoPi * x);
                auto c = cos(2 * twoPi * x);

                switch (Q)
                {
                case 0:
                    v = s;
                    dv = 2 * twoPi * c;
                    break;
                case 1:
                    v = -s;
                    dv = -2 * twoPi * c;
                    break;
                case 2:
                case 3:
                    v = 0;
                    dv = 0;
                    break;
                }

                return std::make_pair(v, dv);
            });

            fillTable(SixSinesWaveProvider::WaveForm::TX8,
                      [](double x, int Q) -> std::pair<double, double> {
                          auto v = 0.0;
                          auto dv = 0.0;
                          if (Q == 0 || Q == 1)
                          {
                              v = 0.5 * (sin(8.0 * M_PI * (x - 0.0625)) + 1);
                              dv = 4.0 * M_PI * cos(8.0 * M_PI * (x - 0.0625));
                          }

                          return {v, dv};
                      });
            fillTable(SixSinesWaveProvider::WaveForm::SPIKY_TX8, [](double x, int Q) {
                double v{0}, dv{0};
                auto s = sin(2 * twoPi * x);
                auto c = cos(2 * twoPi * x);

                auto OCT = Q * 2;
                if (x > .125 && Q == 0)
                    OCT++;
                else if (x > .375 && Q == 1)
                    OCT++;

                switch (OCT)
                {
                case 0:
                    v = 1 - c;
                    dv = 2 * twoPi * s;
                    break;
                case 1:
                    v = 1 + c;
                    dv = -2 * twoPi * s;
                    break;
                case 2:
                    v = 1 + c;
                    dv = -2 * twoPi * s;
                    break;
                case 3:
                    v = -c + 1;
                    dv = 2 * twoPi * s;
                    break;
                default:
                    break;
                }

                return std::make_pair(v, dv);
            });

            // Thanks to https://en.wikipedia.org/wiki/Window_function for these
            // HANN: 0.5 * (1-cos 2pix). Derivative is pi sin 2pix
            fillTable(SixSinesWaveProvider::WaveForm::HANN_WINDOW, [](double x, int Q) {
                auto v = 0.5 * (1.0 - cos(2.0 * M_PI * x));
                auto dv = M_PI * sin(2.0 * M_PI * x);
                return std::make_pair(v, dv);
            });

            auto cosSum = [](double x, double a0, double a1, double a2, double a3,
                             double a4) -> std::pair<double, double> {
                auto v = a0 - a1 * cos(twoPi * x) + a2 * cos(2 * twoPi * x) -
                         a3 * cos(3 * twoPi * x) + a4 * cos(4 * twoPi * x);
                auto dv = -a1 * twoPi * sin(twoPi * x) + a2 * 2 * twoPi * sin(2 * twoPi * x) -
                          a3 * 3 * twoPi * sin(3 * twoPi * x) + a4 * 4 * twoPi * sin(4 * twoPi * x);
                ;
                return std::make_pair(v, dv);
            };
            fillTable(SixSinesWaveProvider::WaveForm::BLACKMAN_HARRIS_WINDOW,
                      [cosSum](double x, int Q) {
                          return cosSum(x, 0.35875, 0.48829, 0.14128, 0.01168, 0.00196);
                      });
            fillTable(SixSinesWaveProvider::WaveForm::HALF_BLACKMAN_HARRIS_WINDOW,
                      [cosSum](double x, int Q) {
                          if (Q == 2 || Q == 3)
                          {
                              return std::make_pair(0.0, 0.0);
                          }
                          auto res = cosSum(x * 2, 0.35875, 0.48829, 0.14128, 0.01168, 0.00196);
                          return std::make_pair(res.first, res.second * 2);
                      });

            // Tukey with alpha 0.15
            fillTable(SixSinesWaveProvider::WaveForm::TUKEY_WINDOW, [](double x, int Q) {
                static constexpr float alpha{0.15};
                auto dSign{1.0};
                if (Q == 2 || Q == 3)
                {
                    x = 1.0 - x;
                    dSign = -1.0;
                }
                auto v{0.0}, dv{1.0};
                if (x < alpha / 2)
                {
                    v = 0.5 * (1 - cos(twoPi * x / alpha));
                    dv = 0.5 * twoPi * sin(twoPi * x / alpha) / alpha;
                }
                else
                {
                    v = 1.0;
                    dv = 0.0;
                }
                return std::make_pair(v, dSign * dv);
            });
        }

        // Fill up interp buffers
        for (int i = 0; i < nPoints; ++i)
        {
            auto t = 1.f * i / (1 << 12);

            auto c0 = 2 * t * t * t - 3 * t * t + 1;
            auto c1 = t * t * t - 2 * t * t + t;
            auto c2 = -2 * t * t * t + 3 * t * t;
            auto c3 = t * t * t - t * t;

            cubicHermiteCoefficients[0][i] = c0;
            cubicHermiteCoefficients[1][i] = c1;
            cubicHermiteCoefficients[2][i] = c2;
            cubicHermiteCoefficients[3][i] = c3;

            linterpCoefficients[0][i] = 1.f - t;
            linterpCoefficients[1][i] = t;
        }
        // Load the SIMD buffers
        for (int i = 0; i < nPoints; ++i)
        {
            for (int WF = 0; WF < NUM_WAVEFORMS; ++WF)
            {
                for (int Q = 0; Q < 4; ++Q)
                {
                    float r alignas(16)[4];
                    r[0] = quadrantTable[WF][Q][i];
                    r[1] = dQuadrantTable[WF][Q][i];
                    r[2] = quadrantTable[WF][Q][i + 1];
                    r[3] = dQuadrantTable[WF][Q][i + 1];
                    simdFullQuad[WF][nPoints * Q + i] = SIMD_MM(load_ps)(r);
                }
            }

            {
                float r alignas(16)[4];
                for (int j = 0; j < 4; ++j)
                {
                    r[j] = cubicHermiteCoefficients[j][i];
                }
                simdCubic[i] = SIMD_MM(load_ps)(r);
            }
        }
        staticsInitialized = true;
        butOnlyForSine = !allWaves;
    }

    void setWaveForm(WaveForm wf)
    {
        auto stwf = size_t(wf);
        if (stwf >= NUM_WAVEFORMS || butOnlyForSine)
            stwf = 0;
        simdQuad = simdFullQuad[stwf];
    }

    double frToPhase{0};
    inline int32_t dPhase(float fr) const
    {
        auto dph = (int32_t)(fr * frToPhase);
        return dph;
    }

    // phase is 26 bits, 12 of fractional, 12 of position in the table and 2 of quadrant
    inline float at(const uint32_t ph) const
    {
        static constexpr uint32_t mask{(1 << 12) - 1};
        static constexpr uint32_t umask{(1 << 14) - 1};

        auto lb = ph & mask;
        auto ub = (ph >> 12) & umask;

        auto q = simdQuad[ub];
        auto c = simdCubic[lb];
        auto r = SIMD_MM(mul_ps(q, c));

        auto h = SIMD_MM(hadd_ps)(r, r);
        auto v = SIMD_MM(hadd_ps)(h, h);
        return SIMD_MM(cvtss_f32)(v);
    }
};
} // namespace sst::basic_blocks::tables
#endif // INCLUDE_SST_BASIC_BLOCKS_TABLES_SIXSINESWAVEPROVIDER_H
