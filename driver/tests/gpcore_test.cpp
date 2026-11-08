// Tes user-mode untuk gpring.h dan gpmix.h (tanpa windows.h: header harus berdiri sendiri).
#include <stdio.h>
#include <string.h>
#include "gpring.h"
#include "gpmix.h"

static int g_total = 0, g_passed = 0;
static const char* g_name = "";
#define CHECK(cond) do { g_total++; if (cond) g_passed++; else printf("FAIL [%s] %s:%d: %s\n", g_name, __FILE__, __LINE__, #cond); } while (0)
#define TEST(name) g_name = #name;

static bool Eq(const BYTE* a, const BYTE* b, size_t n) { return memcmp(a, b, n) == 0; }

static void RingTests()
{
    BYTE store[8], out[16];

    TEST(write_then_read_roundtrip) {
        GpRing r; GpRingInit(&r, store, 8);
        const BYTE in[3] = {1, 2, 3};
        GpRingWrite(&r, in, 3);
        CHECK(GpRingFill(&r) == 3);
        CHECK(GpRingRead(&r, out, 3) == 3);
        CHECK(Eq(out, in, 3));
        CHECK(GpRingFill(&r) == 0);
    }
    TEST(underrun_zero_fills) {
        GpRing r; GpRingInit(&r, store, 8);
        const BYTE in[2] = {9, 9};
        GpRingWrite(&r, in, 2);
        memset(out, 0xAA, sizeof out);
        const BYTE exp[5] = {9, 9, 0, 0, 0};
        CHECK(GpRingRead(&r, out, 5) == 2);
        CHECK(Eq(out, exp, 5));
    }
    TEST(wraparound) {
        GpRing r; GpRingInit(&r, store, 4);
        const BYTE a[3] = {1, 2, 3}, b[3] = {4, 5, 6}, exp[4] = {3, 4, 5, 6};
        GpRingWrite(&r, a, 3);
        CHECK(GpRingRead(&r, out, 2) == 2);
        GpRingWrite(&r, b, 3);
        CHECK(GpRingRead(&r, out, 4) == 4);
        CHECK(Eq(out, exp, 4));
    }
    TEST(overflow_keeps_newest) {
        GpRing r; GpRingInit(&r, store, 4);
        const BYTE a[6] = {1, 2, 3, 4, 5, 6}, exp[4] = {3, 4, 5, 6};
        GpRingWrite(&r, a, 6);
        CHECK(GpRingFill(&r) == 4);
        CHECK(GpRingRead(&r, out, 4) == 4);
        CHECK(Eq(out, exp, 4));
    }
    TEST(overflow_when_partially_full_drops_oldest) {
        GpRing r; GpRingInit(&r, store, 4);
        const BYTE a[3] = {1, 2, 3}, b[2] = {4, 5}, exp[4] = {2, 3, 4, 5};
        GpRingWrite(&r, a, 3);
        GpRingWrite(&r, b, 2);
        CHECK(GpRingFill(&r) == 4);
        CHECK(GpRingRead(&r, out, 4) == 4);
        CHECK(Eq(out, exp, 4));
    }
    TEST(write_exact_capacity_on_partial_ring) {
        GpRing r; GpRingInit(&r, store, 4);
        const BYTE a[2] = {1, 2}, b[4] = {3, 4, 5, 6};
        GpRingWrite(&r, a, 2);
        GpRingWrite(&r, b, 4);
        CHECK(GpRingFill(&r) == 4);
        CHECK(GpRingRead(&r, out, 4) == 4);
        CHECK(Eq(out, b, 4));
    }
    TEST(reset_clears) {
        GpRing r; GpRingInit(&r, store, 4);
        const BYTE a[3] = {1, 2, 3};
        GpRingWrite(&r, a, 3);
        GpRingReset(&r);
        CHECK(GpRingFill(&r) == 0);
        CHECK(GpRingRead(&r, out, 2) == 0);
    }
    TEST(discard_to_keeps_newest) {
        GpRing r; GpRingInit(&r, store, 8);
        const BYTE a[8] = {1, 2, 3, 4, 5, 6, 7, 8}, exp[3] = {6, 7, 8};
        GpRingWrite(&r, a, 8);
        GpRingDiscardTo(&r, 3);
        CHECK(GpRingFill(&r) == 3);
        CHECK(GpRingRead(&r, out, 3) == 3);
        CHECK(Eq(out, exp, 3));
    }
    TEST(discard_to_noop_when_below) {
        GpRing r; GpRingInit(&r, store, 8);
        const BYTE a[2] = {1, 2};
        GpRingWrite(&r, a, 2);
        GpRingDiscardTo(&r, 5);
        CHECK(GpRingFill(&r) == 2);
        CHECK(GpRingRead(&r, out, 2) == 2);
        CHECK(Eq(out, a, 2));
    }
}

static void MixTests()
{
    TEST(read24_sign) {
        const BYTE m1[3] = {0xFF, 0xFF, 0xFF}, mn[3] = {0x00, 0x00, 0x80}, mx[3] = {0xFF, 0xFF, 0x7F};
        CHECK(GpRead24(m1) == -1);
        CHECK(GpRead24(mn) == -8388608);
        CHECK(GpRead24(mx) == 8388607);
    }
    TEST(mono_into_channel_3) {
        LONG acc[16] = {0};
        const BYTE src[6] = {100, 0, 0, 200, 0, 0};
        CHECK(GpMixAccumulate(acc, 2, 8, src, 1, 2));
        for (int i = 0; i < 16; i++) {
            LONG want = (i == 2) ? 100 : (i == 10) ? 200 : 0;
            CHECK(acc[i] == want);
        }
    }
    TEST(stereo_into_5_6) {
        LONG acc[8] = {0};
        const BYTE src[6] = {1, 0, 0, 0xFE, 0xFF, 0xFF}; // 1, -2
        CHECK(GpMixAccumulate(acc, 1, 8, src, 2, 4));
        CHECK(acc[4] == 1);
        CHECK(acc[5] == -2);
        CHECK(acc[0] == 0 && acc[3] == 0 && acc[6] == 0 && acc[7] == 0);
    }
    TEST(two_sources_sum) {
        LONG acc[8] = {0};
        const BYTE mono[3] = {0xE8, 0x03, 0x00};                          // 1000
        const BYTE st[6] = {0xF4, 0x01, 0x00, 0x0C, 0xFE, 0xFF};          // 500, -500
        CHECK(GpMixAccumulate(acc, 1, 8, mono, 1, 0));
        CHECK(GpMixAccumulate(acc, 1, 8, st, 2, 0));
        CHECK(acc[0] == 1500);
        CHECK(acc[1] == -500);
    }
    TEST(mix_saturates_positive) {
        LONG acc[8] = {0};
        const BYTE mx[3] = {0xFF, 0xFF, 0x7F};
        BYTE dst[24] = {0};
        GpMixAccumulate(acc, 1, 8, mx, 1, 0);
        GpMixAccumulate(acc, 1, 8, mx, 1, 0);
        GpMixWrite(acc, 1, 8, dst, 24);
        CHECK(GpRead24(dst) == 8388607);
    }
    TEST(mix_saturates_negative) {
        LONG acc[8] = {0};
        const BYTE mn[3] = {0x00, 0x00, 0x80};
        BYTE dst[24] = {0};
        GpMixAccumulate(acc, 1, 8, mn, 1, 0);
        GpMixAccumulate(acc, 1, 8, mn, 1, 0);
        GpMixWrite(acc, 1, 8, dst, 24);
        CHECK(GpRead24(dst) == -8388608);
    }
    TEST(mix_rejects_out_of_range) {
        LONG acc[8] = {0};
        const BYTE st[6] = {1, 0, 0, 2, 0, 0};
        CHECK(!GpMixAccumulate(acc, 1, 8, st, 2, 7));
        for (int i = 0; i < 8; i++) CHECK(acc[i] == 0);
    }
    TEST(write16) {
        LONG acc[1] = {0x123456};
        BYTE dst[2] = {0};
        GpMixWrite(acc, 1, 1, dst, 16);
        CHECK(dst[0] == 0x34 && dst[1] == 0x12);
    }
    TEST(write24) {
        LONG acc[1] = {0x123456};
        BYTE dst[3] = {0};
        GpMixWrite(acc, 1, 1, dst, 24);
        CHECK(dst[0] == 0x56 && dst[1] == 0x34 && dst[2] == 0x12);
    }
    TEST(write32) {
        LONG acc[1] = {0x123456};
        BYTE dst[4] = {0};
        GpMixWrite(acc, 1, 1, dst, 32);
        CHECK(dst[0] == 0x00 && dst[1] == 0x56 && dst[2] == 0x34 && dst[3] == 0x12);
    }
    TEST(write_negative_all_widths) {
        LONG acc[1] = {-2};
        BYTE d16[2] = {0}, d24[3] = {0}, d32[4] = {0};
        GpMixWrite(acc, 1, 1, d16, 16);
        GpMixWrite(acc, 1, 1, d24, 24);
        GpMixWrite(acc, 1, 1, d32, 32);
        CHECK(d16[0] == 0xFF && d16[1] == 0xFF); // -2 >> 8 == -1 (aritmetika)
        CHECK(d24[0] == 0xFE && d24[1] == 0xFF && d24[2] == 0xFF);
        CHECK(d32[0] == 0x00 && d32[1] == 0xFE && d32[2] == 0xFF && d32[3] == 0xFF);
    }
    TEST(mix_saturates_positive_16_32) {
        LONG acc[1] = {0};
        const BYTE mx[3] = {0xFF, 0xFF, 0x7F};
        BYTE d16[2] = {0}, d32[4] = {0};
        GpMixAccumulate(acc, 1, 1, mx, 1, 0);
        GpMixAccumulate(acc, 1, 1, mx, 1, 0);
        GpMixWrite(acc, 1, 1, d16, 16);
        GpMixWrite(acc, 1, 1, d32, 32);
        CHECK(d16[0] == 0xFF && d16[1] == 0x7F);
        CHECK(d32[0] == 0x00 && d32[1] == 0xFF && d32[2] == 0xFF && d32[3] == 0x7F);
    }
    TEST(mix_saturates_negative_16_32) {
        LONG acc[1] = {0};
        const BYTE mn[3] = {0x00, 0x00, 0x80};
        BYTE d16[2] = {0}, d32[4] = {0};
        GpMixAccumulate(acc, 1, 1, mn, 1, 0);
        GpMixAccumulate(acc, 1, 1, mn, 1, 0);
        GpMixWrite(acc, 1, 1, d16, 16);
        GpMixWrite(acc, 1, 1, d32, 32);
        CHECK(d16[0] == 0x00 && d16[1] == 0x80);
        CHECK(d32[0] == 0x00 && d32[1] == 0x00 && d32[2] == 0x00 && d32[3] == 0x80);
    }
    TEST(write_other_bits_zero_fills) {
        LONG acc[1] = {0x123456};
        BYTE dst[4] = {1, 2, 3, 4};
        GpMixWrite(acc, 1, 1, dst, 8);
        CHECK(dst[0] == 0);
    }
}

int main()
{
    RingTests();
    MixTests();
    printf("%d/%d passed\n", g_passed, g_total);
    return (g_passed == g_total) ? 0 : 1;
}
