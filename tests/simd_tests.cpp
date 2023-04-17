//
// Created by Paul Walker on 4/17/23.
//

#include "catch2.hpp"
#include "smoke_test_sse.h"

#include "sst/basic-blocks/mechanics/simd-ops.h"

#include <iostream>

TEST_CASE("abs_ps", "[simd]")
{
    auto p1 = _mm_set1_ps(13.2);
    auto p2 = _mm_set1_ps(-142.3);
    auto ap1 = sst::basic_blocks::mechanics::abs_ps(p1);
    auto ap2 = sst::basic_blocks::mechanics::abs_ps(p2);

    float res alignas(16)[4];
    _mm_store_ps(res, ap1);
    REQUIRE(res[0] == Approx(13.2).margin(0.0001));

    _mm_store_ps(res, ap2);
    REQUIRE(res[0] == Approx(142.3).margin(0.00001));
}

TEST_CASE("Sums", "[simd]")
{
    float res alignas(16)[4];
    res[0] = 0.1;
    res[1] = 0.2;
    res[2] = 0.3;
    res[3] = 0.4;
    auto val = _mm_load_ps(res);
    REQUIRE(sst::basic_blocks::mechanics::sum_ps_to_float(val) == Approx(1.0).margin(0.00001));
}