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

#ifndef INCLUDE_SST_BASIC_BLOCKS_DSP_ELLIPTICBLEPOSCILLATORS_H
#define INCLUDE_SST_BASIC_BLOCKS_DSP_ELLIPTICBLEPOSCILLATORS_H

/*
 * A special note on licensing: This file can be re-used either
 * in a GPL3 or MIT licensing context. See the information in
 * README.md. If you commit changes to this file, you are also
 * willing to have it re-used in a GPL3 or MIT/BSD context.
 */

#include "elliptic-blep/elliptic-blep.h"
#include "SmoothingStrategies.h"
#include <unordered_map>
#include <algorithm>

namespace sst::basic_blocks::dsp
{

struct CoeffHolder
{
    using payload_t = signalsmith::blep::EllipticBlepPoles<float>;

    static payload_t &getPoleDataForSampleRate(double sampleRate)
    {
        static std::unordered_map<size_t, payload_t> data;

        auto lu = data.find((size_t)sampleRate);
        if (lu == data.end())
        {
            data[(size_t)sampleRate] = payload_t(false, sampleRate);
            lu = data.find((size_t)sampleRate);
        }
        return lu->second;
    }
};

static void prepareEBOscillators(double sr)
{
    // always do 44100 since thats the default setup below even though other
    // SR will swap it out when we call setSampleRate. This keeps us no-arg
    // constructible
    CoeffHolder::getPoleDataForSampleRate(44100);
    CoeffHolder::getPoleDataForSampleRate(sr);
}

template <typename Impl, typename SmoothingStrategy = LagSmoothingStrategy> struct EBOscillatorBase
{
    EBOscillatorBase() : blep(CoeffHolder::getPoleDataForSampleRate(44100))
    {
        SmoothingStrategy::setValueInstant(sratio, 1.0);
        reset();
    }

    /**
     * Set the sample rate; set up the internal blep state.
     */
    void setSampleRate(double sampleRate)
    {
        this->sampleRate = sampleRate;
        this->sampleRateInv = 1.0 / sampleRate;
        blep = signalsmith::blep::EllipticBlep<float>(
            CoeffHolder::getPoleDataForSampleRate(sampleRate));
    }

    /**
     * Reset to initial state. Use when retriggering for instance.
     */
    void reset()
    {
        blep.reset();
        allpass.reset();
        phase = 0;
        sphase = 0;
        SmoothingStrategy::resetFirstRun(dphase);
        SmoothingStrategy::resetFirstRun(sratio);
    }

    void setInitialPhase(float ph, float initSRatio = 0)
    {
        if (initSRatio == 0)
        {
            initSRatio = SmoothingStrategy::getValue(sratio);
        }
        else
        {
            SmoothingStrategy::setValueInstant(sratio, initSRatio);
        }
        this->phase = ph;
        auto sp = this->phase * initSRatio;
        float ip;
        sp = std::modf(sp, &ip);
        this->sphase = sp;
    }

    /**
     * Set the oscillator frequency.
     *
     * @param freqInHz The frequency in hertz
     */
    void setFrequency(float freqInHz)
    {
        assert(sampleRate > 1000); // hit this? call setSampleRate!
        SmoothingStrategy::setTarget(dphase, freqInHz * sampleRateInv);
    }

    /**
     * Set the sync ratio. This sync ratio is a simple multiple of
     * frequency. So 2 means the sync clock runs at double the base
     * frequency and 4 means 4 times. Sync ratio is usually expressed
     * in semitones, but we leave that conversion to the external caller
     * who will have their own appropriate 2^x implementation and would
     * most likely call this with pow(2.0, sync_in_semitones / 12)
     *
     * @param syncRatio the sync ratio as a multiple of frequency
     */
    void setSyncRatio(float syncRatio) { SmoothingStrategy::setTarget(sratio, syncRatio); }

  protected:
    /*
     * At very high (above nyquist) frequencies we can end up occasionally
     * generating a correction more than one sample in the past. The blep
     * library doesn't handle that well. Since we only do this at transitions
     * like phase resets, we can afford to clamp. This means the anti-aliasing
     * of these ridiculously high frequency waves will be worse. But i mean
     * come on, right?!
     *
     * We choose 0.99 etc... since the library also hates 1 but I don't think
     * any of our algorithms here ever generate an exact 1.
     */
    inline float csip(float sip) { return std::clamp(sip, 0.f, 0.999999f); }
    void syncTurnaroundCorrection(float freq, float srval)
    {
        float samplesInPast = (this->phase - 1) / (freq);
        float syncSamplesInPast = samplesInPast * srval;
        float syncAtTurnaround = this->sphase - syncSamplesInPast * freq;
        float valueAtTurnaround = Impl::valueAt(syncAtTurnaround);
        float newValueAtTurnaround = Impl::valueAt(0);
        ;

        float changeAtTurnaround = newValueAtTurnaround - valueAtTurnaround;

        this->blep.add(changeAtTurnaround, 1, this->csip(samplesInPast));
    }

    signalsmith::blep::EllipticBlep<float> blep;
    signalsmith::blep::EllipticBlepAllpass<float> allpass;
    float phase = 0, sphase = 0;

    typename SmoothingStrategy::smoothValue_t sratio, dphase;

    static std::unordered_map<size_t, bool> poled;
    double sampleRate{1}, sampleRateInv{1};
};

template <typename SmoothingStrategy = LagSmoothingStrategy>
struct EBSaw : EBOscillatorBase<EBSaw<SmoothingStrategy>, SmoothingStrategy>
{
    float step()
    {
        auto freq = SmoothingStrategy::getValue(this->dphase);
        auto srval = SmoothingStrategy::getValue(this->sratio);

        this->phase += freq;
        this->sphase += freq * srval;

        this->blep.step();

        if (this->phase >= 1)
        {
            this->syncTurnaroundCorrection(freq, srval);
            this->phase -= 1;
            this->sphase = this->phase * srval;
        }
        else if (this->sphase >= 1)
        {
            float samplesInPast = (this->sphase - 1) / (srval * freq);
            auto lastSampleValue = valueAt(this->sphase - srval * freq);
            auto thisSampleValue = valueAt(this->sphase - 1);

            auto dv = lastSampleValue - thisSampleValue;

            this->sphase = (this->sphase - 1);
            this->blep.add(-2, 1, this->csip(samplesInPast));
        }

        float result = valueAt(this->sphase); // naive sawtooth

        result += this->blep.get(); // add in BLEP residue

        result = this->allpass(result); // (optional) phase correction

        SmoothingStrategy::process(this->dphase);
        SmoothingStrategy::process(this->sratio);

        return result;
    }

    static float valueAt(float sphase) { return 2 * sphase - 1; }
};

template <typename SmoothingStrategy = LagSmoothingStrategy>
struct EBTri : EBOscillatorBase<EBTri<SmoothingStrategy>, SmoothingStrategy>
{
    float step()
    {
        auto freq = SmoothingStrategy::getValue(this->dphase);
        auto srval = SmoothingStrategy::getValue(this->sratio);

        this->phase += freq;
        this->sphase += freq * srval;

        this->blep.step();

        // this turns us around externally
        if (this->phase >= 1)
        {
            if (this->sphase < 1)
            {
                this->syncTurnaroundCorrection(freq, srval);
            }

            this->phase -= 1;
            this->sphase = this->phase * srval;
        }
        else if (this->sphase >= 1)
        {
            this->sphase -= 1;
        }
        else if (srval * freq < 0.25 && lastSPhase < 0.25 && this->sphase >= 0.25)
        {
            float samplesInPast = (this->sphase - 0.25) / (srval * freq);
            auto dDeriv = 8 * srval * freq;
            this->blep.add(dDeriv, 2, this->csip(samplesInPast));
        }
        else if (srval * freq < 0.25 && lastSPhase < 0.75 && this->sphase >= 0.75)
        {
            float samplesInPast = (this->sphase - 0.75) / (srval * freq);
            auto dDeriv = -8 * srval * freq;
            this->blep.add(dDeriv, 2, this->csip(samplesInPast));
        }

        lastSPhase = this->sphase;
        float result = valueAt(this->sphase); // naive sawtooth

        result += this->blep.get();     // add in BLEP residue
        result = this->allpass(result); // (optional) phase correction

        SmoothingStrategy::process(this->dphase);
        SmoothingStrategy::process(this->sratio);

        return result;
    }
    static float valueAt(float sphase)
    {
        auto uphase = sphase - 0.25 + (sphase < 0.25);
        if (uphase < 0.5)
            return 4 * uphase - 1;
        else
            return -4 * uphase + 3;
    }

    float lastSPhase{0};
};

template <typename SmoothingStrategy = LagSmoothingStrategy>
struct EBApproxSin : EBOscillatorBase<EBApproxSin<SmoothingStrategy>, SmoothingStrategy>
{
    float step()
    {
        auto freq = SmoothingStrategy::getValue(this->dphase);
        auto srval = SmoothingStrategy::getValue(this->sratio);

        this->phase += freq;
        this->sphase += freq * srval;

        this->blep.step();

        // this turns us around externally
        if (this->phase >= 1)
        {
            if (this->sphase < 1)
            {
                this->syncTurnaroundCorrection(freq, srval);
            }
            this->phase -= 1;
            this->sphase = this->phase * srval;
        }
        else if (this->sphase >= 1)
        {
            this->sphase -= 1;
        }

        float result = valueAt(this->sphase); // naive sawtooth

        result += this->blep.get();     // add in BLEP residue
        result = this->allpass(result); // (optional) phase correction

        SmoothingStrategy::process(this->dphase);
        SmoothingStrategy::process(this->sratio);

        return result;
    }
    static float valueAt(float sphase)
    {
        float sign{1.f};
        if (sphase > 0.5)
        {
            sphase = sphase - 0.5;
            sign = -1.f;
        }
        auto ang = sphase * 360.0;
        auto res = sign * 4 * ang * (180 - ang) / (40500 - ang * (180 - ang));
        return res;
    }
};

template <typename SmoothingStrategy = LagSmoothingStrategy>
struct EBApproxSemiSin : EBOscillatorBase<EBApproxSemiSin<SmoothingStrategy>, SmoothingStrategy>
{
    float step()
    {
        // TODO: This needs a blep order 2 correction for the derivative change at sphase==0
        auto freq = SmoothingStrategy::getValue(this->dphase);
        auto srval = SmoothingStrategy::getValue(this->sratio);

        this->phase += freq;
        this->sphase += freq * srval;

        this->blep.step();

        if (this->sphase >= 1)
        {
            // so at this point we are moving from pi/2 sin(pi x) the derivative
            // of which is pi^2/2 cos(pi x) so the deriv goes from -pi^2/2 to pi^2/2 or
            // a change of pi^2

            float samplesInPast = (this->sphase - 1) / (srval * freq);
            auto dDeriv = M_PI * M_PI * srval * freq;
            this->blep.add(dDeriv, 2, this->csip(samplesInPast));
            this->sphase -= 1;
        }

        // this turns us around externally
        if (this->phase >= 1)
        {
            if (this->sphase < 1)
            {
                this->syncTurnaroundCorrection(freq, srval);
            }
            this->phase -= 1;
            this->sphase = this->phase * srval;
        }

        float result = valueAt(this->sphase); // naive sawtooth

        result += this->blep.get();     // add in BLEP residue
        result = this->allpass(result); // (optional) phase correction

        SmoothingStrategy::process(this->dphase);
        SmoothingStrategy::process(this->sratio);

        return result;
    }
    static float valueAt(float sphase)
    {
        float sign{1.f};

        sphase *= 0.5;
        auto ang = sphase * 360.0;
        auto res = sign * 4 * ang * (180 - ang) / (40500 - ang * (180 - ang));

        return M_PI * res * 0.5 - 1.0;
    }
};

template <typename SmoothingStrategy = LagSmoothingStrategy>
struct EBPulse : EBOscillatorBase<EBPulse<SmoothingStrategy>, SmoothingStrategy>
{
    EBPulse()
    {
        SmoothingStrategy::setValueInstant(width, 0.5);
        reset();
    }
    void reset()
    {
        level = 1.0;
        EBOscillatorBase<EBPulse<SmoothingStrategy>, SmoothingStrategy>::reset();
        SmoothingStrategy::resetFirstRun(width);
    }

    void setWidth(float w) { SmoothingStrategy::setTarget(width, std::clamp(w, 0.01f, 0.99f)); }

    float step()
    {
        auto freq = SmoothingStrategy::getValue(this->dphase);
        auto srval = SmoothingStrategy::getValue(this->sratio);
        auto wval = SmoothingStrategy::getValue(this->width);

        this->phase += freq;
        this->sphase += freq * srval;

        this->blep.step();

        // sync turnaround
        if (this->phase >= 1)
        {
            if (level == -1)
            {
                float samplesInPast = (this->phase - 1) / (freq);
                // low to high
                this->blep.add(2, 1, this->csip(samplesInPast));
            }

            this->phase -= 1;
            this->sphase = this->phase * srval;
            level = 1;
        }

        // mid width turnaround
        if (this->sphase > wval && level > 0)
        {
            float samplesInPast = (this->sphase - wval) / (srval * freq);

            level = -1;
            // high to low
            this->blep.add(-2, 1, this->csip(samplesInPast));
        }

        if (this->sphase > 1)
        {
            float samplesInPast = (this->sphase - 1) / (srval * freq);

            level = 1;
            // low to high
            this->blep.add(2, 1, this->csip(samplesInPast));
            this->sphase -= 1;
        }

        float result = level;

        result += this->blep.get();     // add in BLEP residue
        result = this->allpass(result); // (optional) phase correction

        // Remove DC offset: pulse wave integral is width * 1 + (1-width) * (-1) = 2*width - 1
        result -= (2.0f * wval - 1.0f);

        SmoothingStrategy::process(this->dphase);
        SmoothingStrategy::process(this->sratio);
        SmoothingStrategy::process(this->width);

        return result;
    }

  protected:
    // SmoothingStrategy::smoothValue_t width;
    typename SmoothingStrategy::smoothValue_t width;
    float level{1};
};
} // namespace sst::basic_blocks::dsp
#endif // ELLIPTICBLEPOSCILLATORS_H
