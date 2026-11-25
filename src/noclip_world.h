// v3.33 NC333: No Clip's collision queries on the game's own functions (GOG v7), for noclip_sim.h's NcWorld. Included by
// src/noclip.inc and by the real-collision harness (staging/v333_feedback/w3_noclip/re/nc_harness.c, which runs them on
// a dumped room collision inside the exe image mapped by native.exe). Game thread only (static aligned buffers: the
// game's collision code uses SSE moves on these vectors).
//   0x561CA0 tree2 present; 0x561CB0(point) 1 = inside solid space of tree2 (the BSP of walls, props, stair blocks,
//   ground); 0x561CD0(line, plane) the plane where the line from its start first enters solid (none when the start is
//   inside solid); 0x41E9C0(line, plane, out) the point on it; 0x561B80(sphere) polygons a sphere touches;
//   0x5339D0(old, wanted, out) the stock walk step (it writes to wanted: a copy is passed); 0x561A70(out[2]) the box of
//   the room's collision (out[0] max, out[1] min).
#ifndef NOCLIP_WORLD_H
#define NOCLIP_WORLD_H

static __declspec(align(16)) float g_nwLine[8], g_nwPlane[4], g_nwHit[4], g_nwSph[8], g_nwPt[4];
static __declspec(align(16)) float g_nwOld[4], g_nwWant[4], g_nwOut[4], g_nwBox[8];

static int NwHasTree2(void) { return ((int(__cdecl *)(void))0x561CA0)() != 0; }

static int NwSolid(void *ctx, float x, float y, float z)
{
    (void)ctx;
    if (!NwHasTree2()) return -1;
    g_nwPt[0] = x; g_nwPt[1] = y; g_nwPt[2] = z; g_nwPt[3] = 1.0f;
    return ((int(__cdecl *)(float *))0x561CB0)(g_nwPt) != 0;
}

// The first surface on a vertical line through (x, z) from yTop down to yBot (y grows downward). 1 = found (*fy).
static int NwFloor(void *ctx, float x, float yTop, float yBot, float z, float *fy)
{
    (void)ctx;
    g_nwLine[0] = x; g_nwLine[1] = yTop; g_nwLine[2] = z; g_nwLine[3] = 1.0f;
    g_nwLine[4] = x; g_nwLine[5] = yBot; g_nwLine[6] = z; g_nwLine[7] = 1.0f;
    if (!((int(__cdecl *)(float *, float *))0x561CD0)(g_nwLine, g_nwPlane)) return 0;
    if (!((int(__cdecl *)(float *, float *, float *))0x41E9C0)(g_nwLine, g_nwPlane, g_nwHit)) return 0;
    *fy = g_nwHit[1];
    return *fy == *fy && *fy - *fy == 0.0f;
}

static void NwStock(void *ctx, const float *old, const float *want, float *out)
{
    (void)ctx;
    g_nwOld[0] = old[0]; g_nwOld[1] = old[1]; g_nwOld[2] = old[2]; g_nwOld[3] = 1.0f;
    g_nwWant[0] = want[0]; g_nwWant[1] = want[1]; g_nwWant[2] = want[2]; g_nwWant[3] = 1.0f;
    g_nwOut[0] = old[0]; g_nwOut[1] = old[1]; g_nwOut[2] = old[2]; g_nwOut[3] = 1.0f;
    ((void(__cdecl *)(float *, float *, float *))0x5339D0)(g_nwOld, g_nwWant, g_nwOut);
    out[0] = g_nwOut[0]; out[1] = g_nwOut[1]; out[2] = g_nwOut[2];
}

static void NwSphereAt(float x, float y, float z, float r)  // {Vec4 centre, radius} as the stock body spheres
{
    g_nwSph[0] = x; g_nwSph[1] = y; g_nwSph[2] = z; g_nwSph[3] = 1.0f; g_nwSph[4] = r;
}
static int NwSphereCount(float x, float y, float z, float r)
{
    NwSphereAt(x, y, z, r);
    return ((int(__cdecl *)(float *))0x561B80)(g_nwSph);
}

// Clear = a floor within 4 under her feet and no polygon at all in her body (50..170 above the feet, r 60; the stock
// collision keeps her 80 from walls, so a stock position is always clear; stair steps under 50 do not count).
static int NwClearAt(const float *p)
{
    float fy;
    if (!NwFloor(0, p[0], p[1] - 20.0f, p[1] + 20.0f, p[2], &fy) || fabsf(fy - p[1]) > 4.0f) return 0;
    return NwSphereCount(p[0], p[1] - 110.0f, p[2], 60.0f) == 0;
}

// The room's collision box into wd (boxValid 0 when the room has no collision tree).
static void NwBox(NcWorld *wd)
{
    int i;
    g_nwBox[0] = g_nwBox[4] = 1e30f;
    g_nwBox[1] = g_nwBox[5] = 1e30f;
    g_nwBox[2] = g_nwBox[6] = 1e30f;
    ((void(__cdecl *)(float *))0x561A70)(g_nwBox);  // out[0] = max, out[1] = min (written only with a tree)
    wd->boxValid = g_nwBox[0] < 1e29f && g_nwBox[4] < 1e29f;
    for (i = 0; i < 3 && wd->boxValid; ++i) {
        const float a = g_nwBox[4 + i], b = g_nwBox[i];
        wd->boxMin[i] = a < b ? a : b;
        wd->boxMax[i] = a < b ? b : a;
        if (!(wd->boxMax[i] - wd->boxMin[i] > 1.0f) || !(wd->boxMax[i] - wd->boxMin[i] < 1e6f)) wd->boxValid = 0;
    }
}

static void NwInit(NcWorld *wd)
{
    wd->solid = NwSolid;
    wd->floorAt = NwFloor;
    wd->stock = NwStock;
    wd->ctx = 0;
    NwBox(wd);
}

#endif
