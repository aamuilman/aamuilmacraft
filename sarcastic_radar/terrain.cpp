#include "terrain.h"
#include "config.h"
#include "palette.h"
#include "radar_shape.h"
#include "tls_root.h"

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "miniz.h"

namespace {

constexpr int    RASTER        = 2 * RADAR_HALF + 1;
constexpr int    CENTER        = RADAR_HALF;
constexpr int    TILES_AXIS    = 2;
constexpr int    TILE_PX       = 256;
constexpr int    MOSAIC_PX     = TILES_AXIS * TILE_PX;
constexpr int    PNG_SIGNATURE = 8;
constexpr size_t PNG_MAX       = 224 * 1024;
constexpr size_t ROW_BYTES     = TILE_PX * 3;
constexpr size_t RAW_BYTES     = TILE_PX * (ROW_BYTES + 1);
constexpr int    BINS          = 4096;
constexpr double EARTH_KM      = 6371.0;
constexpr double EQUATOR_KM    = 40075.016686;
constexpr double kDeg          = 0.017453292519943295;
constexpr double kRad          = 57.29577951308232;
constexpr double kPi           = 3.14159265358979324;
constexpr double MERCATOR_MAX  = 85.05112878;
constexpr double DIAGONAL      = 1.4142135623730951;
constexpr int    SEA_METRES    = 1;

const uint8_t kBayer[8][8] = {
    {  0, 32,  8, 40,  2, 34, 10, 42 },
    { 48, 16, 56, 24, 50, 18, 58, 26 },
    { 12, 44,  4, 36, 14, 46,  6, 38 },
    { 60, 28, 52, 20, 62, 30, 54, 22 },
    {  3, 35, 11, 43,  1, 33,  9, 41 },
    { 51, 19, 59, 27, 49, 17, 57, 25 },
    { 15, 47,  7, 39, 13, 45,  5, 37 },
    { 63, 31, 55, 23, 61, 29, 53, 21 },
};

struct Workspace {
    uint8_t* png;
    uint8_t* raw;
    uint8_t* prevRow;
    uint8_t* lineRow;
    int16_t* tile;
    int16_t* sample;
    float*   shade;
};

void (*g_ink)(int x, int y, uint16_t color) = nullptr;

uint8_t*        g_raster = nullptr;
int16_t*        g_elev   = nullptr;
Workspace       g_ws     = {};
double          g_lat    = 0.0;
double          g_lon    = 0.0;
float           g_radius = 0.0f;
bool            g_built  = false;

void* psram(size_t bytes) {
    return heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void release(Workspace& ws) {
    heap_caps_free(ws.png);
    heap_caps_free(ws.raw);
    heap_caps_free(ws.prevRow);
    heap_caps_free(ws.lineRow);
    heap_caps_free(ws.tile);
    heap_caps_free(ws.sample);
    heap_caps_free(ws.shade);
    ws = Workspace{};
}

bool reserve(Workspace& ws) {
    if (!ws.png)     ws.png     = (uint8_t*)psram(PNG_MAX);
    if (!ws.raw)     ws.raw     = (uint8_t*)psram(RAW_BYTES);
    if (!ws.prevRow) ws.prevRow = (uint8_t*)psram(ROW_BYTES);
    if (!ws.lineRow) ws.lineRow = (uint8_t*)psram(ROW_BYTES);
    if (!ws.tile)    ws.tile    = (int16_t*)psram(TILE_PX * TILE_PX * sizeof(int16_t));
    if (!ws.sample)  ws.sample  = (int16_t*)psram(RASTER * RASTER * sizeof(int16_t));
    if (!ws.shade)   ws.shade   = (float*)psram(RASTER * RASTER * sizeof(float));
    if (!g_elev)     g_elev     = (int16_t*)psram(MOSAIC_PX * MOSAIC_PX * sizeof(int16_t));
    if (!g_raster)   g_raster   = (uint8_t*)psram(RASTER * RASTER);

    return ws.png && ws.raw && ws.prevRow && ws.lineRow && ws.tile &&
           ws.sample && ws.shade && g_elev && g_raster;
}

double widthKm(int zoom, double lat) {
    return EQUATOR_KM * cos(lat * kDeg) / (double)(1u << zoom);
}

int pickZoom(double lat, float radiusKm) {
    const double span = 2.0 * (double)radiusKm * DIAGONAL;
    for (int zoom = 14; zoom >= 4; --zoom) {
        if (ceil(span / widthKm(zoom, lat)) <= 2.0) return zoom;
    }
    return 4;
}

void toTile(double lat, double lon, int zoom, double& x, double& y) {
    const double n = (double)(1u << zoom);
    if (lat > MERCATOR_MAX)  lat = MERCATOR_MAX;
    if (lat < -MERCATOR_MAX) lat = -MERCATOR_MAX;

    const double s = sin(lat * kDeg);
    x = (lon + 180.0) / 360.0 * n;
    y = (0.5 - log((1.0 + s) / (1.0 - s)) / (4.0 * kPi)) * n;
}

void project(double lat, double lon, double bearing, double km,
             double& outLat, double& outLon) {
    const double d  = km / EARTH_KM;
    const double la = lat * kDeg;
    const double lo = lon * kDeg;

    const double la2 = asin(sin(la) * cos(d) + cos(la) * sin(d) * cos(bearing));
    const double lo2 = lo + atan2(sin(bearing) * sin(d) * cos(la),
                                  cos(d) - sin(la) * sin(la2));

    outLat = la2 * kRad;
    outLon = lo2 * kRad;
    while (outLon > 180.0)   outLon -= 360.0;
    while (outLon < -180.0)  outLon += 360.0;
}

uint32_t be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

bool fetchTile(int zoom, int x, int y, uint32_t deadline, size_t& got) {
    got = 0;

    const uint32_t t0 = millis();
    uint32_t t1 = t0 + TERRAIN_TILE_TIMEOUT_MS;
    if ((int32_t)(t1 - deadline) > 0) t1 = deadline;

    if ((int32_t)(t1 - t0) <= 0) {
        Serial.printf("[terrain] tile %d/%d/%d: no time left, skipped\n", zoom, x, y);
        return false;
    }

    char url[128];
    snprintf(url, sizeof(url),
             "https://s3.amazonaws.com/elevation-tiles-prod/terrarium/%d/%d/%d.png",
             zoom, x, y);

    esp_http_client_config_t cfg = {};
    cfg.url = url;
    cfg.cert_pem = kAmazonRootPem;
    cfg.timeout_ms = (int)(t1 - t0);
    cfg.buffer_size = 4096;
    cfg.disable_auto_redirect = true;
    cfg.keep_alive_enable = false;

    esp_http_client_handle_t h = esp_http_client_init(&cfg);
    if (!h) {
        Serial.printf("[terrain] tile %d/%d/%d: init failed\n", zoom, x, y);
        return false;
    }

    esp_http_client_set_header(h, "User-Agent", "M5PaperColor-SarcasticRadar/1.0");

    if (esp_http_client_open(h, 0) != ESP_OK) {
        Serial.printf("[terrain] tile %d/%d/%d: connect failed\n", zoom, x, y);
        esp_http_client_cleanup(h);
        return false;
    }

    const int64_t declared = esp_http_client_fetch_headers(h);
    const int     code     = esp_http_client_get_status_code(h);
    const size_t  want     = declared > 0 ? (size_t)declared : PNG_MAX;

    bool ok = false;

    if (code != 200) {
        Serial.printf("[terrain] tile %d/%d/%d: HTTP %d\n", zoom, x, y, code);
    } else if (declared > (int64_t)PNG_MAX) {
        Serial.printf("[terrain] tile %d/%d/%d: size %lld\n",
                      zoom, x, y, (long long)declared);
    } else {
        int empty = 0;

        while (got < want) {
            if ((int32_t)(millis() - t1) > 0) break;

            const int n = esp_http_client_read(h, (char*)g_ws.png + got, want - got);
            if (n > 0) {
                got += (size_t)n;
                empty = 0;
                continue;
            }

            if (++empty >= 3) {
                if (declared > 0) {
                    int tlsCode = 0;
                    int tlsFlags = 0;
                    esp_http_client_get_and_clear_last_tls_error(h, &tlsCode, &tlsFlags);
                    Serial.printf("[terrain] tile %d/%d/%d: read stopped, errno %d, "
                                  "tls %d, flags %d\n",
                                  zoom, x, y, esp_http_client_get_errno(h),
                                  tlsCode, tlsFlags);
                }
                break;
            }
            delay(5);
        }

        ok = declared > 0 ? got == (size_t)declared : got > 0;
        Serial.printf("[terrain] tile %d/%d/%d: %u %s %lld bytes in %u ms\n",
                      zoom, x, y, (unsigned)got, ok ? "of" : "from",
                      (long long)declared, (unsigned)(millis() - t0));
    }

    esp_http_client_close(h);
    esp_http_client_cleanup(h);
    return ok;
}

bool inflateTile() {
    size_t pos = PNG_SIGNATURE;
    size_t packed = 0;
    bool header = false;

    while (pos + 12 <= PNG_MAX) {
        const uint32_t size = be32(g_ws.png + pos);
        if (pos + 12 + size > PNG_MAX) return false;

        const uint8_t* tag  = g_ws.png + pos + 4;
        const uint8_t* body = g_ws.png + pos + 8;

        if (memcmp(tag, "IHDR", 4) == 0) {
            if (size < 13) return false;
            if (be32(body) != TILE_PX || be32(body + 4) != TILE_PX) return false;
            if (body[8] != 8 || body[9] != 2) return false;
            header = true;
        } else if (memcmp(tag, "IDAT", 4) == 0) {
            memmove(g_ws.png + packed, body, size);
            packed += size;
        } else if (memcmp(tag, "IEND", 4) == 0) {
            break;
        }

        pos += 12 + size;
    }

    if (!header || packed == 0) return false;

    const size_t done = tinfl_decompress_mem_to_mem(g_ws.raw, RAW_BYTES,
                                                    g_ws.png, packed,
                                                    TINFL_FLAG_PARSE_ZLIB_HEADER);
    return done == RAW_BYTES;
}

bool unfilterTile() {
    memset(g_ws.prevRow, 0, ROW_BYTES);

    for (int y = 0; y < TILE_PX; ++y) {
        const uint8_t* row = g_ws.raw + (size_t)y * (ROW_BYTES + 1);
        const uint8_t filter = row[0];
        uint8_t* line = g_ws.lineRow;
        uint8_t* prev = g_ws.prevRow;

        memcpy(line, row + 1, ROW_BYTES);

        switch (filter) {
            case 1:
                for (size_t i = 3; i < ROW_BYTES; ++i) line[i] = (uint8_t)(line[i] + line[i - 3]);
                break;
            case 2:
                for (size_t i = 0; i < ROW_BYTES; ++i) line[i] = (uint8_t)(line[i] + prev[i]);
                break;
            case 3:
                for (size_t i = 0; i < ROW_BYTES; ++i) {
                    const int left = i >= 3 ? line[i - 3] : 0;
                    line[i] = (uint8_t)(line[i] + ((left + prev[i]) >> 1));
                }
                break;
            case 4:
                for (size_t i = 0; i < ROW_BYTES; ++i) {
                    const int a = i >= 3 ? line[i - 3] : 0;
                    const int b = prev[i];
                    const int c = i >= 3 ? prev[i - 3] : 0;
                    const int pa = abs(b - c);
                    const int pb = abs(a - c);
                    const int pc = abs(a + b - 2 * c);
                    const int pred = (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
                    line[i] = (uint8_t)(line[i] + pred);
                }
                break;
            case 0:
                break;
            default:
                return false;
        }

        int16_t* out = g_ws.tile + (size_t)y * TILE_PX;
        for (int x = 0; x < TILE_PX; ++x) {
            const int o = x * 3;
            out[x] = (int16_t)((int)line[o] * 256 + line[o + 1] +
                               (line[o + 2] >> 7) - 32768);
        }

        memcpy(prev, line, ROW_BYTES);
    }
    return true;
}

bool decodeTile() {
    if (!inflateTile()) return false;
    return unfilterTile();
}

bool gather(int zoom, int tx0, int ty0, TerrainResult& out) {
    const uint32_t t0 = millis();
    const uint32_t deadline = t0 + TERRAIN_BUILD_TIMEOUT_MS;

    memset(g_elev, 0, (size_t)MOSAIC_PX * MOSAIC_PX * sizeof(int16_t));

    for (int ty = 0; ty < TILES_AXIS; ++ty) {
        for (int tx = 0; tx < TILES_AXIS; ++tx) {
            if ((int32_t)(millis() - deadline) > 0) {
                snprintf(out.error, sizeof(out.error), "build deadline");
                return false;
            }

            const int tiles = 1 << zoom;
            int x = (tx0 + tx) % tiles;
            if (x < 0) x += tiles;

            const int y = ty0 + ty;
            if (y < 0 || y >= tiles) continue;

            size_t got = 0;
            bool ok = fetchTile(zoom, x, y, deadline, got);
            if (!ok) ok = fetchTile(zoom, x, y, deadline, got);
            if (!ok || !decodeTile()) {
                snprintf(out.error, sizeof(out.error), "tile %d/%d/%d", zoom, x, y);
                return false;
            }

            int16_t* dst = g_elev + (size_t)ty * TILE_PX * MOSAIC_PX + (size_t)tx * TILE_PX;
            for (int row = 0; row < TILE_PX; ++row) {
                memcpy(dst + (size_t)row * MOSAIC_PX,
                       g_ws.tile + (size_t)row * TILE_PX,
                       TILE_PX * sizeof(int16_t));
            }

            ++out.tiles;
        }
    }
    return true;
}

void sampleElevation(int zoom, int tx0, int ty0) {
    const double kmPerPx = (double)g_radius / (double)RADAR_HALF;

    for (int j = 0; j < RASTER; ++j) {
        for (int i = 0; i < RASTER; ++i) {
            const double ox = (double)(i - CENTER);
            const double oy = (double)(j - CENTER);

            double la = 0.0;
            double lo = 0.0;
            project(g_lat, g_lon, atan2(ox, -oy), sqrt(ox * ox + oy * oy) * kmPerPx, la, lo);

            double fx = 0.0;
            double fy = 0.0;
            toTile(la, lo, zoom, fx, fy);

            const int mx = (int)((fx - (double)tx0) * TILE_PX);
            const int my = (int)((fy - (double)ty0) * TILE_PX);

            g_ws.sample[(size_t)j * RASTER + i] =
                (mx >= 0 && my >= 0 && mx < MOSAIC_PX && my < MOSAIC_PX)
                    ? g_elev[(size_t)my * MOSAIC_PX + mx]
                    : 0;
        }
    }
}

void shade() {
    const double az = (double)TERRAIN_SUN_AZ_DEG * kDeg;
    const double al = (double)TERRAIN_SUN_ALT_DEG * kDeg;
    const double sunX = sin(az) * cos(al);
    const double sunY = -cos(az) * cos(al);
    const double sunZ = sin(al);
    const double cell = (double)g_radius * 1000.0 / (double)RADAR_HALF;
    const double k = 1.0 / (8.0 * cell);

    for (int j = 0; j < RASTER; ++j) {
        for (int i = 0; i < RASTER; ++i) {
            float& out = g_ws.shade[(size_t)j * RASTER + i];
            out = 0.0f;

            if (i == 0 || j == 0 || i == RASTER - 1 || j == RASTER - 1) continue;

            const int16_t* up = g_ws.sample + (size_t)(j - 1) * RASTER;
            const int16_t* mid = g_ws.sample + (size_t)j * RASTER;
            const int16_t* dn = g_ws.sample + (size_t)(j + 1) * RASTER;

            const double a = up[i - 1];
            const double b = up[i];
            const double c = up[i + 1];
            const double d = mid[i - 1];
            const double f = mid[i + 1];
            const double g = dn[i - 1];
            const double h = dn[i];
            const double m = dn[i + 1];

            const double zx = ((c + 2.0 * f + m) - (a + 2.0 * d + g)) * k;
            const double zy = ((g + 2.0 * h + m) - (a + 2.0 * b + c)) * k;

            const double nx = -zx;
            const double ny = -zy;
            const double inv = 1.0 / sqrt(nx * nx + ny * ny + 1.0);

            double lit = (nx * sunX + ny * sunY + sunZ) * inv;
            if (lit < 0.0) lit = 0.0;
            if (lit > 1.0) lit = 1.0;

            double slope = sqrt(zx * zx + zy * zy) / (double)TERRAIN_REF_SLOPE;
            if (slope > 1.0) slope = 1.0;

            out = (float)((1.0 - lit) * slope);
        }
    }
}

double stretchLevel(int landCells) {
    if (landCells <= 0) return 0.0;

    static int32_t hist[BINS];
    memset(hist, 0, sizeof(hist));

    for (int n = 0; n < RASTER * RASTER; ++n) {
        if (g_ws.sample[n] <= SEA_METRES) continue;
        int bin = (int)(g_ws.shade[n] * (double)(BINS - 1));
        if (bin < 0) bin = 0;
        if (bin > BINS - 1) bin = BINS - 1;
        ++hist[bin];
    }

    const long target = ((long)landCells * TERRAIN_STRETCH_PERMILLE + 999) / 1000;

    long seen = 0;
    for (int bin = 0; bin < BINS; ++bin) {
        seen += hist[bin];
        if (seen >= target) return (double)bin / (double)(BINS - 1);
    }
    return 1.0;
}

void toDensity() {
    int land = 0;
    for (int n = 0; n < RASTER * RASTER; ++n) {
        if (g_ws.sample[n] > SEA_METRES) ++land;
    }

    const double hi = stretchLevel(land);

    for (int n = 0; n < RASTER * RASTER; ++n) {
        if (g_ws.sample[n] <= SEA_METRES) {
            g_raster[n] = 0;
            continue;
        }

        double v = g_ws.shade[n];
        if (hi > 0.0) {
            v /= hi;
            if (v > 1.0) v = 1.0;
        }

        v = pow(v, (double)TERRAIN_GAMMA);

        int d = (int)lround(v * (double)TERRAIN_MAX_DENSITY);
        if (d < 0) d = 0;
        if (d > TERRAIN_MAX_DENSITY) d = TERRAIN_MAX_DENSITY;
        g_raster[n] = (uint8_t)d;
    }

    int coast = 0;
    for (int j = 0; j < RASTER; ++j) {
        for (int i = 0; i < RASTER; ++i) {
            const size_t n = (size_t)j * RASTER + i;
            if (g_ws.sample[n] <= SEA_METRES) continue;

            const bool near =
                (i > 0          && g_ws.sample[n - 1]     <= SEA_METRES) ||
                (i < RASTER - 1 && g_ws.sample[n + 1]     <= SEA_METRES) ||
                (j > 0          && g_ws.sample[n - RASTER] <= SEA_METRES) ||
                (j < RASTER - 1 && g_ws.sample[n + RASTER] <= SEA_METRES);

            if (near) {
                g_raster[n] = TERRAIN_COAST;
                ++coast;
            }
        }
    }

    Serial.printf("[terrain] raster: land %d, sea %d, coast %d, stretch %.3f\n",
                  land, RASTER * RASTER - land, coast, hi);
}
}

void terrain::setPixel(void (*fn)(int x, int y, uint16_t color)) { g_ink = fn; }

bool terrain::ready() { return g_built && g_raster != nullptr; }

bool terrain::build(double lat, double lon, float radiusKm, TerrainResult& out) {
    out = TerrainResult{};
    out.tried = true;

    const uint32_t t0 = millis();

    if (!reserve(g_ws)) {
        snprintf(out.error, sizeof(out.error), "no PSRAM");
        return false;
    }

    g_lat    = lat;
    g_lon    = lon;
    g_radius = radiusKm;

    const int zoom = pickZoom(lat, radiusKm);

    const double kmPerPx = (double)radiusKm / (double)RADAR_HALF;
    const double reach   = (double)RADAR_HALF * DIAGONAL * kmPerPx;

    double minFx = 1e9;
    double minFy = 1e9;
    double maxFx = -1e9;
    double maxFy = -1e9;

    for (int corner = 0; corner < 4; ++corner) {
        const double ox = (corner & 1) ? (double)RADAR_HALF : -(double)RADAR_HALF;
        const double oy = (corner & 2) ? (double)RADAR_HALF : -(double)RADAR_HALF;

        double la = 0.0;
        double lo = 0.0;
        project(lat, lon, atan2(ox, -oy), reach, la, lo);

        double fx = 0.0;
        double fy = 0.0;
        toTile(la, lo, zoom, fx, fy);

        if (fx < minFx) minFx = fx;
        if (fy < minFy) minFy = fy;
        if (fx > maxFx) maxFx = fx;
        if (fy > maxFy) maxFy = fy;
    }

    const int tx0 = (int)floor(minFx);
    const int ty0 = (int)floor(minFy);

    Serial.printf("[terrain] zoom %d, tiles %dx%d from %d/%d, radius %.0f km\n",
                  zoom, TILES_AXIS, TILES_AXIS, tx0, ty0, (double)radiusKm);

    if (maxFx - minFx > (double)TILES_AXIS || maxFy - minFy > (double)TILES_AXIS) {
        Serial.printf("[terrain] raster spans %.2f x %.2f tiles\n",
                      maxFx - minFx, maxFy - minFy);
    }

    if (!gather(zoom, tx0, ty0, out)) {
        Serial.printf("[terrain] failed: %s after %u ms, %u tiles\n",
                      out.error, (unsigned)(millis() - t0), (unsigned)out.tiles);
        out.ms = millis() - t0;
        return false;
    }

    sampleElevation(zoom, tx0, ty0);

    heap_caps_free(g_elev);
    g_elev = nullptr;

    shade();
    toDensity();

    release(g_ws);

    g_built = true;
    out.ok  = true;
    out.ms  = millis() - t0;

    Serial.printf("[terrain] built in %u ms, PSRAM free %u\n",
                  (unsigned)out.ms, (unsigned)ESP.getFreePsram());
    return true;
}

void terrain::draw(int originX, int originY) {
    if (!g_ink || !g_built || !g_raster) return;

    for (int j = 0; j < RASTER; ++j) {
        for (int i = 0; i < RASTER; ++i) {
            if (!radarshape::inside((float)(i - CENTER), (float)(j - CENTER))) continue;

            const int d = g_raster[(size_t)j * RASTER + i];
            if (d <= 0) continue;

            if (d >= TERRAIN_COAST) {
                g_ink(originX + i, originY + j, palette::FIX);
                continue;
            }

            if (kBayer[j & 7][i & 7] < d) {
                g_ink(originX + i, originY + j, palette::DATA);
            }
        }
    }
}
