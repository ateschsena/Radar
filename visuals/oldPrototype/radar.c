// radar.c  (Windows + raylib) — auto-detect via "RADAR_READY" handshake
// Build (MinGW/MSYS2): gcc radar.c -o radar.exe -lraylib -lopengl32 -lgdi32 -lwinmm
// Run: radar.exe            (auto-detect)
//      radar.exe COM7       (manual override)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOUSER      // removes winuser.h (CloseWindow, DrawTextA, ShowCursor, LoadImageA conflicts)
#define NOGDI       // removes gdi stuff (Rectangle conflict)
#define NOMM        // removes mmsystem (PlaySoundA conflict)

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "raylib.h"


typedef struct {
    HANDLE h;
    char   buf[2048];
    int    len;
    char   portName[32];
} SerialPort;

static int serial_configure(HANDLE h, int baud) {
    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);

    if (!GetCommState(h, &dcb)) return 0;

    dcb.BaudRate = baud;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary  = TRUE;

    // Enable DTR control so we can toggle it (Arduino reset/handshake)
    dcb.fDtrControl = DTR_CONTROL_ENABLE;

    if (!SetCommState(h, &dcb)) return 0;

    COMMTIMEOUTS to;
    memset(&to, 0, sizeof(to));
    to.ReadIntervalTimeout         = 10;
    to.ReadTotalTimeoutConstant    = 10;
    to.ReadTotalTimeoutMultiplier  = 1;
    SetCommTimeouts(h, &to);

    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return 1;
}

static int serial_open(SerialPort *sp, const char *comName, int baud) {
    memset(sp, 0, sizeof(*sp));
    snprintf(sp->portName, sizeof(sp->portName), "%s", comName);

    char fullName[64];
    snprintf(fullName, sizeof(fullName), "\\\\.\\%s", comName);

    sp->h = CreateFileA(fullName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (sp->h == INVALID_HANDLE_VALUE) {
        sp->h = NULL;
        return 0;
    }

    if (!serial_configure(sp->h, baud)) {
        CloseHandle(sp->h);
        sp->h = NULL;
        return 0;
    }

    sp->len = 0;
    return 1;
}

static void serial_close(SerialPort *sp) {
    if (sp->h) {
        CloseHandle(sp->h);
        sp->h = NULL;
    }
    sp->len = 0;
}

static void serial_toggle_dtr(SerialPort *sp) {
    // Force Arduino reset so it reprints "RADAR_READY"
    EscapeCommFunction(sp->h, CLRDTR);
    Sleep(80);
    EscapeCommFunction(sp->h, SETDTR);
    Sleep(80);
    PurgeComm(sp->h, PURGE_RXCLEAR | PURGE_TXCLEAR);
    sp->len = 0;
}

static int serial_readline(SerialPort *sp, char *out, int outCap) {
    char tmp[128];
    DWORD got = 0;

    if (!ReadFile(sp->h, tmp, sizeof(tmp), &got, NULL)) {
        return 0;
    }
    if (got > 0) {
        if (sp->len + (int)got > (int)sizeof(sp->buf)) {
            sp->len = 0; // overflow guard
        }
        memcpy(sp->buf + sp->len, tmp, got);
        sp->len += (int)got;
    }

    for (int i = 0; i < sp->len; i++) {
        if (sp->buf[i] == '\n') {
            int lineLen = i + 1;
            int copyLen = (lineLen < outCap - 1) ? lineLen : (outCap - 1);
            memcpy(out, sp->buf, copyLen);
            out[copyLen] = '\0';

            memmove(sp->buf, sp->buf + lineLen, sp->len - lineLen);
            sp->len -= lineLen;
            return 1;
        }
    }
    return 0;
}

static int line_has_signature(const char *line, const char *sig) {
    return strstr(line, sig) != NULL;
}

static int parse_distance_cm(const char *line, int *cmOut) {
    while (*line && isspace((unsigned char)*line)) line++;
    if (!(*line == '-' || isdigit((unsigned char)*line))) return 0;

    char *end = NULL;
    long v = strtol(line, &end, 10);
    if (end == line) return 0;

    if (v < -1) v = -1;
    if (v > 500) v = 500;

    *cmOut = (int)v;
    return 1;
}

// Try to find a port that prints RADAR_READY after reset
static int serial_autodetect_radar(SerialPort *outSp, int baud, const char *signature) {
    char comName[16];
    char line[256];

    // Scan a reasonable range; you can increase if needed
    for (int i = 1; i <= 64; i++) {
        snprintf(comName, sizeof(comName), "COM%d", i);

        SerialPort sp;
        if (!serial_open(&sp, comName, baud)) continue;

        // Reset the board to force signature printing
        serial_toggle_dtr(&sp);

        DWORD start = GetTickCount();
        int found = 0;

        while (GetTickCount() - start < 2500) { // wait up to 2.5s
            while (serial_readline(&sp, line, sizeof(line))) {
                if (line_has_signature(line, signature)) {
                    found = 1;
                    break;
                }
            }
            if (found) break;
            Sleep(10);
        }

        if (found) {
            *outSp = sp; // keep this open port
            return 1;
        }

        serial_close(&sp);
    }

    return 0;
}

// --- drawing helpers ---
static void draw_arc(Vector2 c, float radius, float degStart, float degEnd, int segments, Color col) {
    float step = (degEnd - degStart) / (float)segments;
    Vector2 prev = {0};

    for (int i = 0; i <= segments; i++) {
        float a = degStart + step * i;
        float theta = (180.0f - a) * DEG2RAD;

        Vector2 p = {
            c.x + radius * cosf(theta),
            c.y - radius * sinf(theta)
        };

        if (i > 0) DrawLineV(prev, p, col);
        prev = p;
    }
}

typedef struct {
    float angleDeg;
    float cm;
    float born;
} Blip;

int main(int argc, char **argv) {
    const int baud = 9600;
    const char *signature = "RADAR_READY";

    SerialPort sp;
    int havePort = 0;

    if (argc >= 2) {
        // Manual override
        if (!serial_open(&sp, argv[1], baud)) {
            fprintf(stderr, "Failed to open %s\n", argv[1]);
            return 1;
        }
        havePort = 1;
    } else {
        // Auto-detect
        if (!serial_autodetect_radar(&sp, baud, signature)) {
            fprintf(stderr, "Could not auto-detect Arduino (signature %s).\n", signature);
            fprintf(stderr, "Tips:\n");
            fprintf(stderr, " - Close Serial Monitor/Plotter.\n");
            fprintf(stderr, " - Try running with a manual port: radar.exe COM3\n");
            return 1;
        }
        havePort = 1;
        printf("Detected Arduino on %s\n", sp.portName);
    }

    // --- UI ---
    const int W = 1000, H = 600;
    InitWindow(W, H, "Radar (distance-only, virtual sweep)");
    SetTargetFPS(60);

    Vector2 center = { W / 2.0f, H - 60.0f };
    float radarRadius = fminf(W / 2.0f - 40.0f, H - 120.0f);
    int maxRangeCm = 300;

    Blip blips[2048];
    int blipCount = 0;

    float sweep = 0.0f;
    float sweepSpeed = 80.0f;
    int sweepDir = 1;

    int latestCm = -1;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();
        float now = (float)GetTime();

        // Read serial lines available this frame
        char line[256];
        int cm;
        while (serial_readline(&sp, line, sizeof(line))) {
            // ignore BOOT / RADAR_READY / any non-numeric lines
            if (parse_distance_cm(line, &cm)) {
                latestCm = cm;

                // store a blip at current sweep angle (virtual)
                if (cm >= 0) {
                    if (blipCount < (int)(sizeof(blips)/sizeof(blips[0]))) {
                        blips[blipCount++] = (Blip){ sweep, (float)cm, now };
                    } else {
                        blips[0] = (Blip){ sweep, (float)cm, now };
                    }
                }
            }
        }

        // Update virtual sweep angle
        sweep += sweepDir * sweepSpeed * dt;
        if (sweep >= 180.0f) { sweep = 180.0f; sweepDir = -1; }
        if (sweep <= 0.0f)   { sweep = 0.0f;   sweepDir =  1; }

        // Fade out old blips
        const float life = 2.0f;
        int write = 0;
        for (int i = 0; i < blipCount; i++) {
            if (now - blips[i].born <= life) blips[write++] = blips[i];
        }
        blipCount = write;

        // Draw
        BeginDrawing();
        ClearBackground(BLACK);

        Color grid = (Color){0, 255, 0, 120};
        Color gridDim = (Color){0, 255, 0, 40};
        Color beam = (Color){0, 255, 0, 200};

        for (int i = 1; i <= 3; i++) {
            float r = radarRadius * (i / 3.0f);
            draw_arc(center, r, 0.0f, 180.0f, 120, grid);
        }

        for (int a = 0; a <= 180; a += 15) {
            float theta = (180.0f - (float)a) * DEG2RAD;
            Vector2 p = { center.x + radarRadius * cosf(theta),
                          center.y - radarRadius * sinf(theta) };
            DrawLineV(center, p, gridDim);
        }

        // Beam
        {
            float theta = (180.0f - sweep) * DEG2RAD;
            Vector2 tip = { center.x + radarRadius * cosf(theta),
                            center.y - radarRadius * sinf(theta) };
            DrawLineEx(center, tip, 2.0f, beam);
        }

        // Blips
        for (int i = 0; i < blipCount; i++) {
            float age = now - blips[i].born;
            float t = 1.0f - (age / life);
            unsigned char alpha = (unsigned char)(50 + 205 * t);

            float r = (blips[i].cm / (float)maxRangeCm) * radarRadius;
            if (r > radarRadius) r = radarRadius;

            float theta = (180.0f - blips[i].angleDeg) * DEG2RAD;
            Vector2 p = { center.x + r * cosf(theta),
                          center.y - r * sinf(theta) };

            DrawCircleV(p, 6.0f, (Color){255, 50, 50, alpha});
        }

        DrawText(TextFormat("Port: %s", sp.portName), 20, H - 40, 20, grid);
        DrawText(TextFormat("Virtual Angle: %d deg", (int)sweep), 200, H - 40, 20, grid);
        DrawText(TextFormat("Distance: %d cm", latestCm), 520, H - 40, 20, grid);

        EndDrawing();
    }

    if (havePort) serial_close(&sp);
    CloseWindow();
    return 0;
}
