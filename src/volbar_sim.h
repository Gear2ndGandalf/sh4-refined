// v3.55 OL56: the volume-bar rule of the Options row draw (R0_REPORT.md section 1.4 and its R5 change 1). The game's row
// draw 0x5254F0 draws a record as a volume bar (0x5238F0, arrows at x - 20 / x + 100) instead of its value text when the
// record's label id [ecx+14h] is 0x25 (BGM Volume) or 0x26 (SE Volume), on any page:
//   0x525672 8B 41 14           mov eax,[ecx+14h]
//   0x525675 83 F8 25           cmp eax,25h
//   0x525678 89 6C 24 38        mov [esp+38h],ebp      (mov: the flags stay)
//   0x52567C 0F 8C ...          jl  text
//   0x525682 83 F8 26 / 0F 8F   cmp eax,26h / jg text
// So a renamed record (Music Volume / Effects Volume: a private label id) would draw its value ids 0x3E..0x48 as text.
// The patch (layout.inc VbInstall, [Options] StockMoves=1 only) writes `call VbLabelCmp; nop` over the first 6 bytes: the
// stub answers "a bar" for the two stock volume inputs (0x524C00 BGM, 0x524CD0 SE) whatever the label, else compares the
// label as the game did, and returns with the flags of `cmp eax,25h` for the jl / jg that follow. Pure code: the stub and
// the rule are tested natively (test\volbar_test.c runs the stub inside the game's own jl / cmp / jg sequence).
#ifndef VOLBAR_SIM_H
#define VOLBAR_SIM_H

#define VB_SITE 0x525672u
#define VB_LEN 6
#define VB_IN_BGM 0x524C00u   // 0x524C00: BGM Volume's input (cfg+0x0D)
#define VB_IN_SE 0x524CD0u    // 0x524CD0: SE Volume's input (cfg+0x0E)
static const unsigned char kVbStock[VB_LEN] = { 0x8B, 0x41, 0x14, 0x83, 0xF8, 0x25 };

// the record (7 dwords: +0 input, +0x14 label id) draws as a bar: by its input (the two stock volume rows), or by the
// stock label ids as the game's own test (RowValWidths and look.inc's glyph count ask this too: a bar has no value text)
static int VbBarRule(unsigned long input, unsigned long label)
{
    return input == VB_IN_BGM || input == VB_IN_SE || label == 0x25 || label == 0x26;
}
// the 6 bytes written at va: E8 rel32 (to the stub) + one nop
static void VbPatchBytes(unsigned long va, unsigned long to, unsigned char out[VB_LEN])
{
    const unsigned long rel = to - (va + 5);
    out[0] = 0xE8;
    out[1] = (unsigned char)(rel & 0xFF);
    out[2] = (unsigned char)(rel >> 8 & 0xFF);
    out[3] = (unsigned char)(rel >> 16 & 0xFF);
    out[4] = (unsigned char)(rel >> 24 & 0xFF);
    out[5] = 0x90;
}

// the stub (0x525672: call here; ecx = the record). eax: 0x25 for a stock volume input, else the label id; the flags of
// `cmp eax,25h`. Uses eax only (the game's own code set eax here too); the stack is balanced at its ret.
static __declspec(naked) void VbLabelCmp(void)
{
    __asm {
        mov eax, [ecx]
        cmp eax, 524C00h         // BGM Volume's input
        je bar
        cmp eax, 524CD0h         // SE Volume's input
        je bar
        mov eax, [ecx + 14h]     // the stolen bytes: the label id
        cmp eax, 25h
        ret
    bar:
        mov eax, 25h
        cmp eax, 25h
        ret
    }
}
#endif
