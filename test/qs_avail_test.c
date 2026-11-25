// v3.33 MENU333: native test of src\qs_avail_sim.h (test\build_qs_avail_test.cmd): the Extra Menu's Quick Save / Quick
// Load row look (hold, first frame, flap anomaly) at 30 / 60 / 120 / 180 fps, the sound-path test, and a model of the
// v3.32 QA report: the read queue pulses for every music / ambience refill (sound reads) and the v3.32 rows, which asked
// "queue empty" every frame, flashed grey; where the refills come back to back they stayed grey.
#include <stdio.h>
#include "../src/qs_avail_sim.h"

static int g_fail, g_n;
#define CHECK(c, ...) do { ++g_n; if (!(c)) { ++g_fail; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const int kFps[4] = { 30, 60, 120, 180 };

// frame k's time in ms at fps, from t0 (unsigned: wraps like GetTickCount)
static unsigned T(unsigned t0, int k, int fps) { return t0 + (unsigned)((double)k * 1000.0 / fps); }

static void TestOpenAndHold(unsigned t0)
{
    int f;
    for (f = 0; f < 4; ++f) {
        const int fps = kFps[f];
        QsAvail a;
        int k, changedAt = -1, r;
        QsAvailOpen(&a, 0, T(t0, 0, fps));
        CHECK(a.shown == 0, "open grey shows grey at once (%d fps)", fps);
        QsAvailOpen(&a, 1, T(t0, 0, fps));
        CHECK(a.shown == 1, "open available shows available at once (%d fps)", fps);
        // a sustained change to grey at frame 10: the look follows QS_HOLD_MS later (within one frame)
        for (k = 1; k < 10 * fps; ++k) {
            r = QsAvailStep(&a, k < 10, T(t0, k, fps));
            if ((r & 1) && changedAt < 0) changedAt = k;
            CHECK(!(r & 2), "no anomaly for one change (%d fps, frame %d)", fps, k);
        }
        {
            const double ms = (double)(changedAt - 10) * 1000.0 / fps;
            CHECK(changedAt > 0 && ms >= QS_HOLD_MS - 0.01 && ms < QS_HOLD_MS + 1000.0 / fps + 1.0,
                  "grey after %.1f ms of grey (hold %d, %d fps)", ms, QS_HOLD_MS, fps);
        }
        CHECK(a.shown == 0, "ends grey (%d fps)", fps);
    }
}

// pulses shorter than the hold never change the look; a 10 Hz flap is reported once a window, the look held
static void TestPulsesAndFlap(unsigned t0)
{
    int f;
    for (f = 0; f < 4; ++f) {
        const int fps = kFps[f];
        QsAvail a;
        int k, changes = 0, anomalies = 0;
        QsAvailOpen(&a, 1, T(t0, 0, fps));
        for (k = 1; k < 20 * fps; ++k) {  // 20 s: a 150 ms grey pulse every 2 s
            const unsigned ms = T(t0, k, fps) - t0;
            const int raw = (ms % 2000u) >= 150u;
            const int r = QsAvailStep(&a, raw, T(t0, k, fps));
            changes += r & 1;
            anomalies += (r >> 1) & 1;
        }
        CHECK(changes == 0 && anomalies == 0, "150 ms pulses every 2 s: %d look changes, %d anomalies (%d fps)", changes,
              anomalies, fps);
        QsAvailOpen(&a, 1, T(t0, 0, fps));
        changes = anomalies = 0;
        for (k = 1; k < 5 * fps; ++k) {  // 5 s of a 10 Hz flap (50 ms grey / 50 ms clear)
            const unsigned ms = T(t0, k, fps) - t0;
            const int r = QsAvailStep(&a, (ms / 50u) % 2u == 0, T(t0, k, fps));
            changes += r & 1;
            anomalies += (r >> 1) & 1;
        }
        CHECK(changes == 0, "10 Hz flap: the look held (%d changes, %d fps)", changes, fps);
        CHECK(anomalies >= 4 && anomalies <= 6, "10 Hz flap for 5 s: %d anomaly lines (one a window, %d fps)", anomalies, fps);
    }
}

static void TestSoundPath(void)
{
    CHECK(QsIsSoundPath("data\\sound\\bgm.sh4", 0x200), "bgm.sh4 is sound");
    CHECK(QsIsSoundPath("C:\\GAME\\DATA\\SE.SH4", 0x200), "SE.SH4 is sound (case)");
    CHECK(QsIsSoundPath("bd.sh4", 0x200), "bd.sh4 is sound");
    CHECK(!QsIsSoundPath("data\\tw00.bin", 0x200), "tw00.bin is data");
    CHECK(!QsIsSoundPath("data\\x.sh4x", 0x200), ".sh4x is data");
    CHECK(!QsIsSoundPath("sh4", 0x200), "short name is data");
    CHECK(!QsIsSoundPath("", 0x200), "empty is data");
    {
        char p[8] = { 'a', 'b', '.', 's', 'h', '4', 'x', 'x' };  // no terminator within max 6: "ab.sh4"
        CHECK(QsIsSoundPath(p, 6), "bounded scan stops at max");
    }
}

// The v3.32 QA model. raw332 = the hard state && queue empty (sound refills count), raw333 = the hard state (sound reads
// never count; a data-file load is a passing state and does not grey). Count the frames each look shows grey.
static void TestQaModel(unsigned t0)
{
    int f;
    for (f = 0; f < 4; ++f) {
        const int fps = kFps[f];
        int k, grey332 = 0, flashes332 = 0, prev332 = 1, grey333 = 0, grey332s = 0, grey333s = 0;
        QsAvail a, b;
        QsAvailOpen(&a, 1, T(t0, 0, fps));
        QsAvailOpen(&b, 1, T(t0, 0, fps));
        for (k = 1; k < 30 * fps; ++k) {
            const unsigned ms = T(t0, k, fps) - t0;
            const int soundRead = (ms % 2500u) < 120u;        // a refill every 2.5 s, 120 ms in the queue
            const int soundBusy = (ms % 400u) < 330u;         // "Subway": streams refilling back to back
            const int raw332 = !soundRead;
            const int raw332s = !soundBusy;
            int s;
            grey332 += !raw332;
            grey332s += !raw332s;
            if (prev332 && !raw332) ++flashes332;
            prev332 = raw332;
            QsAvailStep(&a, 1, T(t0, k, fps));               // v3.33: sound reads are not a guard at all
            s = QsAvailStep(&b, 1, T(t0, k, fps));
            (void)s;
            grey333 += !a.shown;
            grey333s += !b.shown;
        }
        CHECK(flashes332 >= 11 && grey332 > 0, "v3.32 model flashes grey %d times in 30 s (%d fps)", flashes332, fps);
        CHECK(grey332s > 30 * fps * 7 / 10, "v3.32 model grey %d of %d frames in the busy room (%d fps)", grey332s, 30 * fps, fps);
        CHECK(grey333 == 0 && grey333s == 0, "v3.33: no grey frame (%d / %d, %d fps)", grey333, grey333s, fps);
    }
}

int main(void)
{
    TestOpenAndHold(1000);
    TestOpenAndHold(0xFFFFFF00u);  // GetTickCount wraps during the test
    TestPulsesAndFlap(5000);
    TestPulsesAndFlap(0xFFFFF000u);
    TestSoundPath();
    TestQaModel(0);
    printf("qs_avail_test: %d checks, %d failed\n", g_n, g_fail);
    return g_fail != 0;
}
