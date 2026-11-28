// v3.59 FR359: Room 302's window look-out view (records 0x69843C / 0x698468, update 0x54C190) at the true rate: its
// per-tick constants (research staging\v359_feedback\w8_rate\research_window_view.md; every byte string read from the
// stock exe, test\fr359_test.c checks the values against cutrate_sim.h's rules). The same row shape as fps_tail_table.h
// ({site, operand offset, type, bytes, stock bytes, values at 60 / 120 / 180, pair group}). m = N / 30, k = 30 / N.
// Every row is used only while the view runs (its own group 0x20 tasks; the shared constants are redirected per site).
static const RateTail kWv359[] = {
    // the view's camera 0x54BAE0: the mouse's per-frame delta x 1/24 (x m, as the peephole rows 0x5089A3; the stick is rate-aware)
    { 0x54BB6D, 2, RT_PTR, 6, { 0xD8, 0x0D, 0xD4, 0xD6, 0x5B, 0x00 }, { 0.0833334f, 0.1666668f, 0.2500002f }, 0 },
    { 0x54BBBA, 2, RT_PTR, 6, { 0xD8, 0x0D, 0xD4, 0xD6, 0x5B, 0x00 }, { 0.0833334f, 0.1666668f, 0.2500002f }, 0 },
    // lane traffic 0x54CD90: the jerk terms (speed and acceleration stay in 30 Hz units; the integrations get x k in caves)
    { 0x54D045, 2, RT_PTR, 6, { 0xD8, 0x25, 0xB0, 0x7D, 0x5B, 0x00 }, { 0.025f, 0.0125f, 0.00833333333f }, 0 },
    { 0x54D0A4, 2, RT_PTR, 6, { 0xD8, 0x05, 0x9C, 0x72, 0x5B, 0x00 }, { 0.5f, 0.25f, 0.166666667f }, 0 },
    { 0x54D0B6, 2, RT_PTR, 6, { 0xD8, 0x05, 0x60, 0x74, 0x5B, 0x00 }, { 0.1f, 0.05f, 0.0333333333f }, 0 },
    // lane spawner 0x54E1A0: rand() % M + B tick delays (state 0 and state 1, two lanes)
    { 0x54E204, 1, RT_I32, 5, { 0xB9, 0x0F, 0x00, 0x00, 0x00 }, { 29, 57, 85 }, 0 },
    { 0x54E26D, 1, RT_I32, 5, { 0xB9, 0x0F, 0x00, 0x00, 0x00 }, { 29, 57, 85 }, 0 },
    { 0x54E42E, 1, RT_I32, 5, { 0xB9, 0x0F, 0x00, 0x00, 0x00 }, { 29, 57, 85 }, 0 },
    { 0x54E461, 1, RT_I32, 5, { 0xB9, 0x0F, 0x00, 0x00, 0x00 }, { 29, 57, 85 }, 0 },
    { 0x54E20B, 2, RT_I8, 3, { 0x83, 0xC2, 0x08 }, { 16, 32, 48 }, 0 },
    { 0x54E27A, 2, RT_I8, 3, { 0x83, 0xC2, 0x08 }, { 16, 32, 48 }, 0 },
    { 0x54E435, 2, RT_I8, 3, { 0x83, 0xC2, 0x08 }, { 16, 32, 48 }, 0 },
    { 0x54E468, 2, RT_I8, 3, { 0x83, 0xC2, 0x08 }, { 16, 32, 48 }, 0 },
    { 0x54E216, 1, RT_I32, 5, { 0xB9, 0x7F, 0x00, 0x00, 0x00 }, { 253, 507, 887 }, 0 },  // + 2 x what the imm8 base lacks
    { 0x54E28D, 1, RT_I32, 5, { 0xB9, 0x7F, 0x00, 0x00, 0x00 }, { 253, 507, 887 }, 0 },
    { 0x54E440, 1, RT_I32, 5, { 0xB9, 0x7F, 0x00, 0x00, 0x00 }, { 253, 507, 887 }, 0 },
    { 0x54E473, 1, RT_I32, 5, { 0xB9, 0x7F, 0x00, 0x00, 0x00 }, { 253, 507, 887 }, 0 },
    { 0x54E21D, 2, RT_I8, 3, { 0x83, 0xC2, 0x20 }, { 64, 127, 127 }, 0 },
    { 0x54E29A, 2, RT_I8, 3, { 0x83, 0xC2, 0x20 }, { 64, 127, 127 }, 0 },
    { 0x54E447, 2, RT_I8, 3, { 0x83, 0xC2, 0x20 }, { 64, 127, 127 }, 0 },
    { 0x54E47A, 2, RT_I8, 3, { 0x83, 0xC2, 0x20 }, { 64, 127, 127 }, 0 },
    // lane lights (draw path): the blink phase step and the lamp-dimming chance (rand & 0x3FF) < 8 (1 at 180: 25 % rarer)
    { 0x54D5BC, 2, RT_I8, 3, { 0x83, 0xC0, 0x10 }, { 8, 4, 3 }, 0 },
    { 0x54DC9E, 2, RT_I8, 3, { 0x83, 0xF8, 0x08 }, { 4, 2, 1 }, 0 },
    // the rare model F000F047 0x54F770: phase step a tick, phase from game ticks away, the turn a tick
    { 0x54F81E, 2, RT_PTR, 6, { 0xD8, 0x05, 0xC0, 0x79, 0x5B, 0x00 }, { 5e-05f, 2.5e-05f, 1.66666667e-05f }, 0 },
    { 0x54F8FE, 2, RT_PTR, 6, { 0xD8, 0x0D, 0xC0, 0x79, 0x5B, 0x00 }, { 5e-05f, 2.5e-05f, 1.66666667e-05f }, 0 },
    { 0x54F854, 2, RT_PTR, 6, { 0xD8, 0x0D, 0x30, 0x75, 0x5B, 0x00 }, { 0.005f, 0.0025f, 0.00166666667f }, 0 },
    // the rare drop model F000F016 0x54F990: wait rand() % 150 + 90 (the counter starts at -90 (m - 1)), rise, spin
    { 0x54FC82, 1, RT_I32, 5, { 0xB9, 0x96, 0x00, 0x00, 0x00 }, { 299, 597, 895 }, 1 },
    { 0x54FC8A, 6, RT_I32, 10, { 0xC7, 0x05, 0x80, 0x58, 0x0A, 0x01, 0x00, 0x00, 0x00, 0x00 }, { -90, -270, -450 }, 1 },
    { 0x69949C, 0, RT_F32, 4, { 0x00, 0x00, 0x70, 0x41 }, { 7.5f, 3.75f, 2.5f }, 0 },
    { 0x54FBCE, 2, RT_PTR, 6, { 0xD8, 0x0D, 0x60, 0x74, 0x5B, 0x00 }, { 0.1f, 0.05f, 0.0333333333f }, 2 },
    { 0x54FBD5, 2, RT_PTR, 6, { 0xD8, 0x25, 0xC8, 0x7D, 0x5B, 0x00 }, { 0.05f, 0.025f, 0.0166666667f }, 2 },
    // the birds 0x550300: speed table x k (private .data), tangents speed x 15 -> x 15 m (the same curve)
    { 0x699664, 0, RT_F32, 4, { 0x00, 0x00, 0xE0, 0x40 }, { 3.5f, 1.75f, 1.16666667f }, 3 },
    { 0x69966C, 0, RT_F32, 4, { 0x00, 0x00, 0x20, 0x41 }, { 5.0f, 2.5f, 1.66666667f }, 3 },
    { 0x699674, 0, RT_F32, 4, { 0x00, 0x00, 0xC0, 0x40 }, { 3.0f, 1.5f, 1.0f }, 3 },
    { 0x69967C, 0, RT_F32, 4, { 0x00, 0x00, 0x10, 0x41 }, { 4.5f, 2.25f, 1.5f }, 3 },
    { 0x699684, 0, RT_F32, 4, { 0x00, 0x00, 0xC0, 0x41 }, { 12.0f, 6.0f, 4.0f }, 3 },
    { 0x55036D, 2, RT_PTR, 6, { 0xD8, 0x0D, 0xE0, 0x7E, 0x5B, 0x00 }, { 30.0f, 60.0f, 90.0f }, 3 },
    { 0x5504C5, 2, RT_PTR, 6, { 0xD8, 0x0D, 0xE0, 0x7E, 0x5B, 0x00 }, { 30.0f, 60.0f, 90.0f }, 3 },
    // the bird spawner 0x550B10: first delay, the sky prefill (ticks ahead, gaps), the re-arm delays
    { 0x550BAA, 1, RT_I32, 5, { 0xB9, 0x1F, 0x00, 0x00, 0x00 }, { 61, 121, 181 }, 0 },
    { 0x550BBD, 2, RT_I8, 3, { 0x83, 0xC2, 0x10 }, { 32, 64, 96 }, 0 },
    { 0x550BD1, 1, RT_I32, 5, { 0xBF, 0x00, 0x04, 0x00, 0x00 }, { 2048, 4096, 6144 }, 4 },
    { 0x550BEE, 1, RT_I32, 5, { 0xB9, 0x1F, 0x00, 0x00, 0x00 }, { 61, 121, 181 }, 4 },
    { 0x550BF5, 1, RT_I32, 5, { 0xB8, 0xE0, 0xFF, 0xFF, 0xFF }, { -64, -128, -192 }, 4 },
    { 0x550C02, 1, RT_I32, 5, { 0xB9, 0x3F, 0x00, 0x00, 0x00 }, { 125, 249, 373 }, 4 },
    { 0x550C09, 1, RT_I32, 5, { 0xB8, 0xD0, 0xFF, 0xFF, 0xFF }, { -96, -192, -288 }, 4 },
    { 0x550C3C, 1, RT_I32, 5, { 0xB9, 0x1F, 0x00, 0x00, 0x00 }, { 61, 123, 311 }, 5 },  // + 2 x what the imm8 base lacks
    { 0x550C48, 1, RT_I32, 5, { 0xB9, 0x3F, 0x00, 0x00, 0x00 }, { 125, 251, 503 }, 5 },
    { 0x550C58, 2, RT_I8, 3, { 0x83, 0xC2, 0x20 }, { 64, 127, 127 }, 5 },
};
#define NWV359 ((int)(sizeof kWv359 / sizeof kWv359[0]))
// the caves (ratephase.inc): {site, length, stock bytes}
static const struct { DWORD va; int len; BYTE orig[8]; const char *what; } kWv359Cave[] = {
    { 0x54D203, 6, { 0xD8, 0xAF, 0xF8, 0x01, 0x00, 0x00 }, "lane traffic: braking, accel -= f x k" },
    { 0x54D4A2, 6, { 0xD8, 0x87, 0xF4, 0x01, 0x00, 0x00 }, "lane traffic: speed += accel x k" },
    { 0x54D4ED, 6, { 0xD9, 0x87, 0xF4, 0x01, 0x00, 0x00 }, "lane traffic: position += speed x k x dir" },
    { 0x54DA6A, 7, { 0x83, 0xC4, 0x10, 0x41, 0x83, 0xE1, 0x07 }, "lane lights: the light cell steps on 30 Hz ticks" },
    { 0x54C5BD, 5, { 0xA3, 0xD0, 0x57, 0x0A, 0x01 }, "window frame: the click bump ends after 1/30 s" },
    { 0x54C5D8, 6, { 0x89, 0x35, 0xD0, 0x57, 0x0A, 0x01 }, "window frame: the click bump starts" },
};
#define NWV359CAVE ((int)(sizeof kWv359Cave / sizeof kWv359Cave[0]))
