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

#ifndef INCLUDE_SST_BASIC_BLOCKS_MECHANICS_STRING_OPS_H
#define INCLUDE_SST_BASIC_BLOCKS_MECHANICS_STRING_OPS_H

#include <algorithm>
#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

/*
 * Parsing a number a user typed, without the active locale deciding what they
 * meant.
 *
 * std::atof, std::stof and std::strtod all follow the C locale's LC_NUMERIC. In
 * a comma-decimal locale "0.5" stops at the dot and silently becomes 0, and in a
 * dot-decimal locale "0,5" does the same. Either way the fraction disappears
 * with no error and the user gets a value they did not type. Since what someone
 * types depends on their keyboard and habits rather than on our process locale,
 * we accept either separator.
 *
 * Where the platform can tell us what the user's conventions actually are we use
 * that, via the std::numpunct facet, which also reports the digit grouping - so
 * "1.234,5" and "1,234.5" both come out as 1234.5 rather than one of them
 * silently truncating. Note the facet's grouping differs by region (en_IN
 * reports {3,2} for lakh/crore, pl_PL groups with a space, not a dot), which is
 * exactly why we read it rather than hardcoding ',' and '.'.
 *
 * When the locale cannot tell us anything - which is the common case for a
 * plugin under a host on macOS and Linux, where LC_* is frequently unset and
 * std::locale("") collapses to the classic locale - a lone separator is read as
 * a decimal point unless it sits where a group separator would.
 */

namespace sst::basic_blocks::mechanics
{

/*
 * What a locale believes about separators. Defaults describe a locale that tells
 * us nothing, which is the safe "accept either separator" footing.
 */
struct SeparatorConventions
{
    char decimal{'.'};
    char thousands{','};
    bool grouped{false}; // does this locale actually group digits at all
};

/*
 * What a given locale believes about separators. Pulled out so a caller doing
 * multi-locale work can build conventions for a locale other than the one the
 * process happens to be in, and so the tests can be deterministic.
 *
 * A locale that carries no numpunct facet is reported as knowing nothing, which
 * lands on the accept-either-separator path rather than on a guess.
 */
inline SeparatorConventions separatorConventionsFor(const std::locale &loc)
{
    SeparatorConventions c;

    try
    {
        const auto &np = std::use_facet<std::numpunct<char>>(loc);
        c.decimal = np.decimal_point();
        c.thousands = np.thousands_sep();
        c.grouped = !np.grouping().empty();
    }
    catch (const std::exception &)
    {
    }

    return c;
}

namespace detail
{
/*
 * Reading the environment's locale is not free and std::locale("") throws
 * outright if the environment names a locale that does not exist, so resolve it
 * once behind a function-local static and treat any failure as "we know
 * nothing", which lands on the accept-either-separator path below.
 */
inline const SeparatorConventions &nativeConventions()
{
    static const SeparatorConventions conventions = []() -> SeparatorConventions {
        try
        {
            // std::locale("") reads the environment and throws outright if it
            // names a locale that does not exist, so this stays guarded.
            return separatorConventionsFor(std::locale(""));
        }
        catch (const std::exception &)
        {
            return SeparatorConventions{};
        }
    }();
    return conventions;
}
} // namespace detail

/*
 * Parse the leading number out of a string, independent of the active locale.
 * Returns nullopt when there is no number to read - it never throws, unlike the
 * std::sto* family.
 *
 * Like std::atof this reads a prefix and ignores whatever follows, so "0.20 s"
 * and "3 whoozits" parse fine and keep unit-suffixed type-ins working. Sign and
 * exponent are handled; hex-float, inf and nan literals are not treated as
 * numbers.
 */
inline std::optional<double> parseNumber(std::string_view s, const SeparatorConventions &nat)
{
    size_t i{0};
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
        ++i;

    std::string norm;
    norm.reserve(s.size() + 1);

    if (i < s.size() && (s[i] == '+' || s[i] == '-'))
        norm += s[i++];

    auto isSeparator = [&nat](char c) {
        return c == '.' || c == ',' || (nat.grouped && c == nat.thousands);
    };

    std::string body;
    while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') || isSeparator(s[i])))
        body += s[i++];

    if (body.empty())
        return std::nullopt;

    // Decide which separator, if any, is the decimal point. Two different
    // separators means the rightmost is the decimal and the other is grouping,
    // which resolves "1,234.5" and "1.234,5" without needing the locale at all.
    constexpr auto npos = std::string::npos;
    auto lastDot = body.rfind('.');
    auto lastComma = body.rfind(',');
    char decimalChar{0};

    if (lastDot != npos && lastComma != npos)
    {
        decimalChar = (lastDot > lastComma) ? '.' : ',';
    }
    else
    {
        char only{0};
        if (lastDot != npos)
            only = '.';
        else if (lastComma != npos)
            only = ',';
        else if (nat.grouped && body.find(nat.thousands) != npos)
            only = nat.thousands;

        if (only)
        {
            // A separator that repeats is grouping ("1.234.567"). A single one is
            // the decimal point unless the locale groups with that character and
            // it sits exactly a group away from the end, where "1,234" is far more
            // likely to be a grouped thousand than a fraction.
            auto occurrences = std::count(body.begin(), body.end(), only);
            auto trailingDigits = body.size() - body.rfind(only) - 1;
            bool looksGrouped =
                occurrences > 1 || (nat.grouped && only == nat.thousands && trailingDigits == 3);

            if (!looksGrouped)
                decimalChar = only;
        }
    }

    bool sawDigit{false};
    for (char c : body)
    {
        if (c >= '0' && c <= '9')
        {
            norm += c;
            sawDigit = true;
        }
        else if (decimalChar && c == decimalChar)
        {
            norm += '.';
        }
        // anything else here is a group separator, which carries no value
    }

    if (!sawDigit)
        return std::nullopt;

    // an exponent, if the user wrote one, passes through untouched
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E'))
    {
        auto j = i + 1;
        std::string expPart;
        if (j < s.size() && (s[j] == '+' || s[j] == '-'))
            expPart += s[j++];
        std::string expDigits;
        while (j < s.size() && s[j] >= '0' && s[j] <= '9')
            expDigits += s[j++];
        if (!expDigits.empty())
        {
            norm += 'e';
            norm += expPart;
            norm += expDigits;
        }
    }

    std::istringstream is{norm};
    is.imbue(std::locale::classic());

    double res{0.0};
    is >> res;
    if (is.fail())
        return std::nullopt;

    return res;
}

/*
 * The everyday entry points, which ask the platform what the user's conventions
 * are. Pass SeparatorConventions explicitly when you need a deterministic answer
 * regardless of the environment - the tests do exactly that.
 */
inline std::optional<double> parseNumber(std::string_view s)
{
    return parseNumber(s, detail::nativeConventions());
}

inline std::optional<double> parseNumber(const std::string &s)
{
    return parseNumber(std::string_view(s), detail::nativeConventions());
}

inline std::optional<double> parseNumber(const std::string &s, const SeparatorConventions &nat)
{
    return parseNumber(std::string_view(s), nat);
}

inline std::optional<double> parseNumber(const char *s)
{
    if (!s)
        return std::nullopt;
    return parseNumber(std::string_view(s), detail::nativeConventions());
}

inline std::optional<double> parseNumber(const char *s, const SeparatorConventions &nat)
{
    if (!s)
        return std::nullopt;
    return parseNumber(std::string_view(s), nat);
}

} // namespace sst::basic_blocks::mechanics

#endif // INCLUDE_SST_BASIC_BLOCKS_MECHANICS_STRING_OPS_H
