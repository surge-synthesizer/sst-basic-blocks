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

#ifndef INCLUDE_SST_BASIC_BLOCKS_TABLES_TEMPOSYNCSUPPORT_H
#define INCLUDE_SST_BASIC_BLOCKS_TABLES_TEMPOSYNCSUPPORT_H

#include <array>
#include <cstdint>
#include <algorithm>
#include <string>
#include <optional>
#include <cmath>
#include <cstdio>
#include <cctype>

namespace sst::basic_blocks::tables::temposync
{

/*
 * EXPONENT CONVENTION (used everywhere - there is deliberately no second scale):
 *
 *   A Note's `exponent` is base-2 in BEATS with a QUARTER NOTE == 0, i.e.
 *   beats = 2^exponent.
 *
 *     exponent   note        beats
 *       -5       1/128       1/32
 *       -1       1/8         1/2
 *        0       1/4         1     (quarter)
 *        1       1/2         2
 *        2       whole       4
 *        5       8 wholes    32
 *
 *   A modifier scales the duration: triplet x2/3, dotted x3/2.
 *
 * Two encodings ("flavors") map a parameter's float onto a Note:
 *   ZERO_ONE   - a 0..1 index into the 33-entry discrete table (struct ZeroOne).
 *   TWO_TO_THE - the legacy Surge 2^x form. Its math currently still lives in
 *                ParamMetaData; it will move here as a sibling of ZeroOne in a
 *                later pass.
 *
 * Everything that does not depend on the encoding - the Note struct, secondsFor,
 * and compact/verbose rendering - is shared below and is flavor-free.
 */
enum class Flavor
{
    TWO_TO_THE,
    ZERO_ONE
};

struct Note
{
    enum Modifier : int8_t
    {
        Triplet = -1,
        Straight = 0,
        Dotted = 1
    };

    int exponent{0}; // quarter == 0; beats = 2^exponent
    Modifier modifier{Straight};
    double beats{1.0}; // duration in beats = 2^exponent * (2/3 | 1 | 3/2)

    constexpr Note() = default;
    constexpr Note(int e, Modifier m) : exponent(e), modifier(m), beats(beatsFor(e, m)) {}

    static constexpr double beatsFor(int e, Modifier m)
    {
        double base = (e >= 0) ? double(1 << e) : 1.0 / double(1 << (-e));
        double f = (m == Triplet) ? (2.0 / 3.0) : (m == Dotted) ? (3.0 / 2.0) : 1.0;
        return base * f;
    }

    // A true zero-duration stage (e.g. an envelope segment of 0 s). beats == 0 is
    // the sentinel; exponent/modifier are unused for a zero Note.
    static constexpr Note zeroStage()
    {
        Note n;
        n.beats = 0.0;
        return n;
    }
    constexpr bool isZero() const { return beats == 0.0; }
};

// Duration in seconds at a given tempo. Flavor-free: a beat = 60/bpm seconds,
// so seconds = beats * 60 / bpm. (1/4 note @ 60bpm -> 1s.)
constexpr double secondsFor(const Note &n, double bpm) { return n.beats * 60.0 / bpm; }

/*
 * Compact ("1/4 T") and verbose ("1/4 triplet") rendering, shared by both
 * flavors. Sub-whole durations (exponent < 2) read as "1/X"; whole-or-longer as
 * "NW" / "N whole notes". Dotted in the whole regime resolves to its integer
 * whole-count (dotted 2W -> "3W", dotted 8W -> "12W") since that reads better; a
 * dotted single whole (1.5 wholes) has no integer form so it keeps the dotted
 * form. Triplets never resolve (N*2/3 is never integral here).
 */
inline std::string toStringCompact(const Note &n)
{
    if (n.isZero())
        return "0";
    int e = n.exponent;
    if (e < 2) // sub-whole: 1/X note
    {
        std::string s = "1/" + std::to_string(1 << (2 - e));
        if (n.modifier == Note::Triplet)
            s += " T";
        else if (n.modifier == Note::Dotted)
            s += " D";
        return s;
    }
    int nWhole = 1 << (e - 2);
    if (n.modifier == Note::Straight)
        return std::to_string(nWhole) + "W";
    if (n.modifier == Note::Triplet)
        return std::to_string(nWhole) + "W T";
    if (e >= 3) // dotted of an even whole-count is integral
        return std::to_string(nWhole * 3 / 2) + "W";
    return std::to_string(nWhole) + "W D"; // dotted single whole == 1.5 wholes
}

inline std::string toString(const Note &n)
{
    if (n.isZero())
        return "0 s";
    int e = n.exponent;
    if (e < 2)
    {
        std::string s = "1/" + std::to_string(1 << (2 - e));
        if (n.modifier == Note::Triplet)
            s += " triplet";
        else if (n.modifier == Note::Dotted)
            s += " dotted";
        else
            s += " note";
        return s;
    }
    int nWhole = 1 << (e - 2);
    if (n.modifier == Note::Straight)
        return nWhole == 1 ? "whole note" : std::to_string(nWhole) + " whole notes";
    if (n.modifier == Note::Triplet)
        return nWhole == 1 ? "whole triplet" : std::to_string(nWhole) + " whole triplets";
    if (e >= 3)
        return std::to_string(nWhole * 3 / 2) + " whole notes";
    return "dotted whole note";
}

// Parse a temposync notation string back into a Note - the inverse of toString
// (and toStringCompact). Handles verbose words ("1/4 triplet", "whole note",
// "dotted whole note"), compact suffixes ("1/4 T", "2W", "1W D"), and the
// resolved-dotted whole forms ("3 whole notes" == dotted 2-whole, "12W" == dotted
// 8-whole). Flavor-neutral; returns nullopt on anything unrecognized.
inline std::optional<Note> fromString(const std::string &s)
{
    std::string numPart, restPart;
    bool inNum{true};
    for (const auto &c : s)
    {
        if (inNum && ((c >= '0' && c <= '9') || c == '/' || c == ' '))
            numPart += c;
        else
        {
            inNum = false;
            if (c != ' ')
                restPart += std::toupper(c);
        }
    }

    // Modifier: verbose words win; otherwise the compact T/D/. suffix (the 'W'
    // whole-marker is stripped first so "1W T" -> "T").
    Note::Modifier mod = Note::Straight;
    if (restPart.find("TRIPLET") != std::string::npos)
        mod = Note::Triplet;
    else if (restPart.find("DOTTED") != std::string::npos ||
             restPart.find('.') != std::string::npos)
        mod = Note::Dotted;
    else
    {
        std::string core = restPart;
        core.erase(std::remove(core.begin(), core.end(), 'W'), core.end());
        if (core == "T")
            mod = Note::Triplet;
        else if (core == "D")
            mod = Note::Dotted;
    }

    bool hasWhole = restPart.find('W') != std::string::npos;

    bool hasDigit = false;
    for (char c : numPart)
        if (c >= '0' && c <= '9')
        {
            hasDigit = true;
            break;
        }

    int num = 1, den = 1;
    if (hasDigit)
    {
        auto slpos = numPart.find('/');
        if (slpos == std::string::npos)
            num = std::stoi(numPart);
        else
        {
            num = std::stoi(numPart.substr(0, slpos));
            den = std::stoi(numPart.substr(slpos + 1));
        }
    }
    else if (!hasWhole) // no number and not a bare "whole ..." form
        return std::nullopt;
    if (hasDigit && num == 0)
        return Note::zeroStage();
    if (num <= 0 || den <= 0)
        return std::nullopt;

    // exponent on the quarter==0 scale from the literal note value
    int exponent = 1 - (int)std::floor(std::log2((double)den / (2.0 * num)));

    // A whole-count that is not a power of two and has no modifier word is a
    // resolved-dotted form: "3 whole notes" == dotted 2-whole (count = base*3/2).
    auto isPow2 = [](int n) { return n > 0 && (n & (n - 1)) == 0; };
    if (mod == Note::Straight && den == 1 && !isPow2(num))
    {
        if (num % 3 == 0 && isPow2(2 * num / 3))
        {
            mod = Note::Dotted;
            exponent = 2 + (int)std::lround(std::log2((double)(2 * num / 3)));
        }
        else
            return std::nullopt;
    }

    return Note(exponent, mod);
}

/*
 * ZERO_ONE table. 11 bases (exponent -5..5: 1/128 note .. 8 whole notes) x 3
 * modifiers = 33 entries, sorted by duration. Within each octave cell the order
 * is straight(k) < triplet(k+1) < dotted(k) < straight(k+1), so we emit in that
 * interleave instead of sorting. Generation is in detail:: free functions: an
 * in-class static constexpr array cannot be initialized by a member function of
 * the same (still-incomplete) class.
 */
namespace detail
{
inline constexpr int zMinExp{-5};
inline constexpr int zMaxExp{5};
inline constexpr int zNEntries{(zMaxExp - zMinExp + 1) * 3}; // 33

constexpr std::array<Note, zNEntries> buildEntries()
{
    std::array<Note, zNEntries> e{};
    int i = 0;
    e[i++] = Note(zMinExp, Note::Triplet);
    for (int k = zMinExp; k < zMaxExp; ++k)
    {
        e[i++] = Note(k, Note::Straight);
        e[i++] = Note(k + 1, Note::Triplet);
        e[i++] = Note(k, Note::Dotted);
    }
    e[i++] = Note(zMaxExp, Note::Straight);
    e[i++] = Note(zMaxExp, Note::Dotted);
    return e;
}

// Reverse map keyed by baseIndex * 3 + (modifier + 1) for O(1) (base,unit) lookup.
constexpr std::array<int, zNEntries> buildReverse(const std::array<Note, zNEntries> &e)
{
    std::array<int, zNEntries> r{};
    for (int idx = 0; idx < zNEntries; ++idx)
        r[(e[idx].exponent - zMinExp) * 3 + (e[idx].modifier + 1)] = idx;
    return r;
}

// 1/beats per entry, so DSP rate math is a lookup with no runtime division.
constexpr std::array<double, zNEntries> buildInverseBeats(const std::array<Note, zNEntries> &e)
{
    std::array<double, zNEntries> r{};
    for (int idx = 0; idx < zNEntries; ++idx)
        r[idx] = 1.0 / e[idx].beats;
    return r;
}

inline constexpr auto zEntries{buildEntries()};
inline constexpr auto zReverse{buildReverse(zEntries)};
inline constexpr auto zInverseBeats{buildInverseBeats(zEntries)};
} // namespace detail

struct ZeroOne
{
    using Modifier = Note::Modifier;

    static constexpr int minExp{detail::zMinExp};
    static constexpr int maxExp{detail::zMaxExp};
    static constexpr int nBases{maxExp - minExp + 1}; // 11
    static constexpr int nEntries{detail::zNEntries}; // 33
    static constexpr float denom{nEntries - 1};       // 32.0 exactly

    static constexpr const std::array<Note, nEntries> &entries{detail::zEntries};

    // ---- 0..1 <-> index ----
    static constexpr int indexFromZeroOne(float v)
    {
        // round by hand (+0.5 then truncate via int cast): std::lround isn't constexpr
        return (int)(std::clamp(v, 0.f, 1.f) * denom + 0.5f);
    }
    static constexpr float zeroOneFromIndex(int idx) { return idx / denom; }
    static constexpr float snap(float v) { return zeroOneFromIndex(indexFromZeroOne(v)); }

    // ---- value <-> note (provider interface: fromFloat/toFloat) ----
    // zeroAtBottom: when true the lowest slot (index 0) is a true "0 s" stage
    // (Note::zeroStage) instead of the smallest note, for envelope segments that
    // want a real zero. The other 32 slots are unchanged; the smallest note
    // (1/128 triplet) is simply unavailable in that mode.
    static constexpr Note fromFloat(float v, bool zeroAtBottom = false)
    {
        int idx = indexFromZeroOne(v);
        if (zeroAtBottom && idx == 0)
            return Note::zeroStage();
        return entries[idx];
    }
    static constexpr float toFloat(const Note &n, bool zeroAtBottom = false)
    {
        if (n.isZero())
            return zeroOneFromIndex(0); // index 0; valid as zero only when zeroAtBottom
        (void)zeroAtBottom;
        int e = std::clamp(n.exponent, minExp, maxExp);
        return zeroOneFromIndex(detail::zReverse[(e - minExp) * 3 + (n.modifier + 1)]);
    }

    // Duration in beats for a 0..1 value (snaps internally). Returns 0 at the
    // zero stage (index 0 when zeroAtBottom), so DSP can do seconds = beats*60/bpm
    // and get a true 0 s segment.
    static constexpr double beatsFromFloat(float v, bool zeroAtBottom = false)
    {
        return fromFloat(v, zeroAtBottom).beats;
    }

    // 1/beats for a 0..1 value (snaps internally), as a table lookup. Returns 0
    // at the zero stage so DSP can treat it as "no rate" (instant).
    static constexpr double inverseBeatsFromFloat(float v, bool zeroAtBottom = false)
    {
        int idx = indexFromZeroOne(v);
        if (zeroAtBottom && idx == 0)
            return 0.0;
        return detail::zInverseBeats[idx];
    }

    // Continuous duration in beats: piecewise-linear in beats between adjacent
    // grid notes, so a modulated value glides instead of snapping cell-to-cell.
    // Exact at grid points (index/denom), where it equals beatsFromFloat, so an
    // unmodulated (UI-snapped) value reproduces the snapped duration with no
    // special case. Interpolating beats (rather than inverseBeats) keeps the
    // zero stage well behaved: the index 0->1 cell ramps up from 0.
    static constexpr double interpolatedBeatsFromFloat(float v, bool zeroAtBottom = false)
    {
        float xp = std::clamp(v, 0.f, 1.f) * denom;
        int xpi = (int)xp;
        if (xpi >= nEntries - 1)
            return entries[nEntries - 1].beats;
        float xpf = xp - xpi;
        double b0 = (zeroAtBottom && xpi == 0) ? 0.0 : entries[xpi].beats;
        double b1 = entries[xpi + 1].beats;
        return (1.0 - xpf) * b0 + xpf * b1;
    }

    // ---- independent base/unit editor ----
    static constexpr int baseIndexOf(int idx) { return entries[idx].exponent - minExp; } // 0..10
    static constexpr Modifier modifierOf(int idx) { return entries[idx].modifier; }
    static constexpr int indexFor(int baseIndex, Modifier m)
    {
        return detail::zReverse[baseIndex * 3 + (m + 1)];
    }
};

/*
 * TWO_TO_THE flavor: the legacy Surge 2^x temposync encoding. Same provider
 * interface as ZeroOne - fromFloat(float)->Note and toFloat(Note)->float - so
 * ParamMetaData can render via the shared toString(Note)/fromString and swap
 * flavors seamlessly. The magic constants (0.51, 0.6, 1.167, 1.41) are
 * historical Surge values; preserve verbatim. ParamMetaData keeps its own
 * asserts and temposyncMultiplier handling on its side.
 */
struct TwoToThe
{
    static inline float snap(float f)
    {
        float a, b = modff(f, &a);
        if (b < 0)
        {
            b += 1.f;
            a -= 1.f;
        }
        b = powf(2.0f, b);

        if (b > 1.41f)
        {
            b = log2(1.5f);
        }
        else if (b > 1.167f)
        {
            b = log2(1.3333333333f);
        }
        else
        {
            b = 0.f;
        }

        return a + b;
    }

    // Decode a (snapped) legacy 2^x value into a Note. Derived from the old
    // temposyncNotation branch logic: with the fractional part normalized to
    // [0,1), the integer part `a` plus the modifier offset give the note -
    // straight: a+1, triplet (off ~0.415): a+2, dotted (off ~0.585): a+1.
    static inline Note fromFloat(float g)
    {
        float fa;
        float b = modff(g, &fa);
        int a = (int)fa;
        if (b < 0)
        {
            b += 1.f;
            a -= 1;
        }
        if (b > 0.5f)
            return Note(a + 1, Note::Dotted);
        if (b > 0.2f)
            return Note(a + 2, Note::Triplet);
        return Note(a + 1, Note::Straight);
    }

    // Encode a Note back to the legacy 2^x value. Matches the old
    // valueFromTemposyncNotation exactly (rate-like, i.e. WITHOUT the
    // temposyncMultiplier, which ParamMetaData still applies on its side). The
    // 0.51 / 0.6 offsets are historical Surge values; preserve verbatim.
    static inline float toFloat(const Note &n)
    {
        float pfrac = (float)(1 - n.exponent);
        if (n.modifier == Note::Triplet)
            pfrac += 0.51f;
        else if (n.modifier == Note::Dotted)
            pfrac -= 0.6f;
        return snap(pfrac);
    }
};

} // namespace sst::basic_blocks::tables::temposync

#endif // INCLUDE_SST_BASIC_BLOCKS_TABLES_TEMPOSYNCSUPPORT_H
