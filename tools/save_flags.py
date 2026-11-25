# Read-only: lists the save slots of the copied save banks / quick saves with stage, room and the two story bits
# the Quick Save guard reads (0x13D815B bit 5 = story byte 0xBB, 0x13D8113 bit 5 = byte 0x73). File slot = image[0x10:].
import sys, os, struct, collections
D = sys.argv[1]
def recover(seg):
    n = len(seg)
    K = collections.Counter((seg[i] - seg[i - 3]) & 255 for i in range(3, n)).most_common(1)[0][0]
    b = [collections.Counter((seg[j] - K * (j // 3)) & 255 for j in range(p, n, 3)).most_common(1)[0][0] for p in range(3)]
    return bytes(seg[j] ^ ((b[j % 3] + K * (j // 3)) & 255) for j in range(n))
def show(tag, img, off):  # off = where the image's +0x10 sits
    st, rm = struct.unpack_from('<II', img, off + 0x04)
    story = img[off + 0x70: off + 0x70 + 0x140]
    print('%-22s stage %2d room %3d  BB.5 %d  73.5 %d  BB=%02X' % (tag, st, rm, (story[0xBB] >> 5) & 1, (story[0x73] >> 5) & 1, story[0xBB]))
for f in ('sh4.sav', 'sh4_bank2.sav', 'sh4_bank3.sav'):
    d = open(os.path.join(D, f), 'rb').read()
    for s in range(30):
        seg = d[0x670 + s * 0x5A30: 0x670 + (s + 1) * 0x5A30]
        p = recover(seg)
        show('%s #%02d' % (f, s + 1), p, 0)
for k in (1, 2, 3):
    q = open(os.path.join(D, 'quick', 'qs%d.bin' % k), 'rb').read()
    show('qs%d gtime %d' % (k, struct.unpack_from('<I', q, 64 + 4)[0]), q, 64 + 0x10)
