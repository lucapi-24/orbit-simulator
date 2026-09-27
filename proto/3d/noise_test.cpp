// Test de validación del ruido 3D — FUERA del juego.
// Escribe un perfil 1D (PNG) y un mapa global (PNG), e imprime estadísticas
// para calibrar AMP_BASE sin tener que abrir el juego.
//
//   cmake --build build && .\build\noise_test.exe
//
// Salidas: proto/3d/out/perfil_1d.png, proto/3d/out/mapa_512.png
#include "raylib.h"
#include "Noise3D.hpp"
#include <stdio.h>

// Perfil 1D: recorre un gran círculo y dibuja la altura. Si la cordillera se
// ve continua y con valles real, el fBm está bien.
static void TestPerfil(void) {
    const int N = 2000, W = 2000, H = 400;
    float minH =  1e9f, maxH = -1e9f, sum = 0.0f;
    float *h = (float *)malloc(sizeof(float) * N);

    for (int i = 0; i < N; i++) {
        // Gran círculo en el ecuador: dir = (cos t, 0, sin t)
        float t = (float)i / N * 2.0f * PI;
        Vector3 dir = { cosf(t), 0.0f, sinf(t) };
        h[i] = Altitud3D(dir);
        if (h[i] < minH) minH = h[i];
        if (h[i] > maxH) maxH = h[i];
        sum += h[i];
    }

    printf("── PERFIL 1D (gran circulo ecuatorial) ──\n");
    printf("  min    %8.2f km\n", minH);
    printf("  max    %8.2f km\n", maxH);
    printf("  medio  %8.2f km\n", sum / N);
    printf("  rango  %8.2f km\n", maxH - minH);

    // Cuántos cambios de signo de pendiente hay: mide si el terreno es "ruido
    // nervioso" (demasiado) o "cordillera" (bien).
    int cambios = 0;
    float prevSlope = 0.0f;
    for (int i = 1; i < N; i++) {
        float slope = h[i] - h[i-1];
        if (prevSlope != 0.0f && ((slope > 0) != (prevSlope > 0))) cambios++;
        prevSlope = slope;
    }
    printf("  cambios de pendiente: %d  (%.1f por 1000 muestras)\n",
           cambios, cambios * 1000.0f / N);

    // PNG: altura → Y, agua en azul, tierra en verde/gris
    Image img = GenImageColor(W, H, { 20, 30, 60, 255 });
    for (int x = 0; x < W; x++) {
        float alt = h[x * N / W];
        int y = (int)((alt - minH) / (maxH - minH) * (H - 1));
        y = H - 1 - y;
        Color c;
        if (alt < 0.0f) {
            // Bathymetría: más profundo = más oscuro
            int d = (int)(-alt / (fabsf(minH) + 1e-6f) * 120.0f);
            c = { (unsigned char)(30 + d/3), (unsigned char)(60 + d/2), (unsigned char)(140 + d), 255 };
        } else {
            // Lowlands verdes → picos nevados
            float t = alt / (maxH + 1e-6f);
            if (t < 0.35f)      c = {  60, (unsigned char)(110 + t*180),  70, 255 };
            else if (t < 0.70f) c = { (unsigned char)(90 + (t-0.35f)*220), (unsigned char)(130 - (t-0.35f)*90), 80, 255 };
            else if (t < 0.90f) c = { (unsigned char)(150 + (t-0.70f)*250), (unsigned char)(120 - (t-0.70f)*80), (unsigned char)(100 - (t-0.70f)*40), 255 };
            else                c = { 250, 250, 255, 255 };
        }
        ImageDrawPixel(&img, x, y, c);
    }
    ExportImage(img, "proto/3d/out/perfil_1d.png");
    UnloadImage(img);
    free(h);
    printf("  -> proto/3d/out/perfil_1d.png\n\n");
}

// Mapa global: el mismo ruido proyectado sobre la esfera. Comprueba que hay
// continentes (no solo ruido uniforme) y que no hay costura ni distorsión polar.
static void TestMapa(void) {
    const int W = 512, H = 256;
    Image img = GenImageColor(W, H, { 0, 0, 0, 255 });
    float minH =  1e9f, maxH = -1e9f, sumH = 0.0f;
    int tierraPix = 0;

    for (int y = 0; y < H; y++) {
        float lat = ((float)y / H - 0.5f) * PI;          // -π/2 .. π/2
        float cl = cosf(lat), sl = sinf(lat);
        for (int x = 0; x < W; x++) {
            float lon = ((float)x / W - 0.5f) * 2.0f * PI;
            // dir desde (lon, lat) — la geometría de la esfera, sin UVs
            Vector3 dir = { cl * cosf(lon), sl, cl * sinf(lon) };
            float alt = Altitud3D(dir);
            if (alt < minH) minH = alt;
            if (alt > maxH) maxH = alt;
            sumH += alt;
            if (alt > 0.0f) tierraPix++;

            Color c;
            if (alt < 0.0f) {
                int d = (int)(-alt / (fabsf(minH) + 1e-6f) * 100.0f);
                c = { (unsigned char)(20 + d/4), (unsigned char)(50 + d/3), (unsigned char)(130 + d/2), 255 };
            } else {
                float t = alt / (maxH + 1e-6f);
                if (t < 0.30f)      c = {  50, (unsigned char)(100 + t*200),  60, 255 };
                else if (t < 0.65f) c = { (unsigned char)(80 + (t-0.30f)*230), (unsigned char)(130 - (t-0.30f)*100), 75, 255 };
                else if (t < 0.88f) c = { (unsigned char)(140 + (t-0.65f)*300), (unsigned char)(115 - (t-0.65f)*70), 95, 255 };
                else                c = { 245, 248, 255, 255 };
            }
            ImageDrawPixel(&img, x, y, c);
        }
    }
    ExportImage(img, "proto/3d/out/mapa_512.png");
    UnloadImage(img);

    float fracTierra = 100.0f * tierraPix / (W * H);
    printf("── MAPA GLOBAL 512x256 ──\n");
    printf("  min       %8.2f km\n", minH);
    printf("  max       %8.2f km\n", maxH);
    printf("  medio     %8.2f km\n", sumH / (W * H));
    printf("  tierra    %8.1f %% de la superficie   (Tierra real: 29%%)\n", fracTierra);
    printf("  -> proto/3d/out/mapa_512.png\n\n");
}

// Diagnóstico numérico: sustituye a "mirar el PNG" y caza fallos que el ojo
// no ve (costuras, polos, muros verticales, octavas perdidas en float32).
static void TestContinuidad(void) {
    printf("── CONTINUIDAD Y ESTABILIDAD ──\n");
    // OJO con las unidades: PLANET_R está en km. 10 m = 0.01 km.
    const float paso10m = 0.01f / PLANET_R;
    const float paso1km =  1.00f / PLANET_R;

    // 1) COSTURA: lon=-π y lon=+π son el mismo punto. Deben dar EXACTAMENTE igual.
    float lat = 0.7f, cl = cosf(lat), sl = sinf(lat);
    float a1 = Altitud3D({ cl * cosf(-PI), sl, cl * sinf(-PI) });
    float a2 = Altitud3D({ cl * cosf( PI), sl, cl * sinf( PI) });
    printf("  costura (lon=±π):  %+.9f vs %+.9f  →  delta %.3e km\n",
           a1, a2, fabsf(a1 - a2));

    // 2) POLO — la pregunta correcta NO es "¿varía en el polo?" (a 105 m del
    //    polo, λ=45 m ⇒ 2.3 celdas de la octava fina, DEBE variar), sino
    //    "¿hayCostura al CRUZAR el polo?". Un ruido 2D con UVsKvwould reventar
    //    aquí; uno 3D es continuo por construcción.
    float maxDeltaPolo = 0.0f;
    float hPolo = Altitud3D({ 0.0f, 1.0f, 0.0f });        // lat = +90°
    for (int i = 1; i <= 20000; i++) {                   // 200 km cruzando el polo
        float lat = PI * 0.5f + paso1km * (float)i;
        float lon = 1.1f;
        Vector3 d = { cosf(lat) * cosf(lon), sinf(lat), cosf(lat) * sinf(lon) };
        float h = Altitud3D(d);
        if (fabsf(h - hPolo) > maxDeltaPolo) maxDeltaPolo = fabsf(h - hPolo);
        hPolo = h;
    }
    printf("  polo (delta/1km):     %.4f km  →  %s\n", maxDeltaPolo,
           maxDeltaPolo < 0.5f ? "OK sin costura" : "FALLO costura polar");

    // 3) CONTINUIDAD a paso de 10 m. Un salto real (acantilado) aparecería
    //    como delta de km aquí. Importante: cubrir TIERRA, no solo el fondo
    //    oceánico (que tiene 2 octavas suaves y siempre da ~0, engañoso).
    float maxDelta = 0.0f, deltaEn = 0.0f; int deltaRuta = 0;
    int ruta = 0;
    for (int m = 0; m < 16; m++) {
        float lon = (float)m / 16.0f * 2.0f * PI;
        for (int p = 0; p < 9; p++) {                 // 9 paralelos
            float lat = -1.2f + 0.3f * (float)p;
            Vector3 d0 = { cosf(lat) * cosf(lon), sinf(lat), cosf(lat) * sinf(lon) };
            float h0 = Altitud3D(d0);
            if (h0 < 0.5f) continue;                 // solo rutas sobre tierra alta
            ruta++;
            for (int i = 1; i <= 10000; i++) {        // 100 km por ruta
                float dLat = paso10m * (float)i;
                Vector3 d = { cosf(lat + dLat) * cosf(lon), sinf(lat + dLat), cosf(lat + dLat) * sinf(lon) };
                float h = Altitud3D(d);
                if (fabsf(h - h0) > maxDelta) {
                    maxDelta = fabsf(h - h0);
                    deltaEn = (float)i * 0.01f; deltaRuta = ruta;
                }
                h0 = h;
            }
        }
    }
    printf("  rutas sobre tierra:   %d\n", ruta);
    printf("  delta max (paso 10m):  %.4f km (ruta %d, a los %.0f km)  →  %s\n",
           maxDelta, deltaRuta, deltaEn,
           maxDelta < 0.05f ? "OK sin saltos" : "FALLO hay discontinuidad");

    // 4) PENDIENTE: ¿caminable? OJO con la base de la diferencia: tiene que
    //    ser MUCHO menor que λ de la octava fina (45 m). Con base de 30 m
    //    estás por debajo de Nyquist y la diferencia finita devuelve ~0.
    //    Usamos 2 m. 60° = escalera.
    const float dLat = 0.00000033f;   // 2 m de arco
    float maxSlope = 0.0f; float slopeEn = 0.0f;
    int sondas = 0;
    for (int m = 0; m < 12; m++) {
        float lon = (float)m / 12.0f * 2.0f * PI;
        for (int i = 0; i < 20000; i++) {
            float lat = -1.3f + 0.13f * (float)(i % 20);
            if (lat > 1.3f) continue;
            Vector3 p    = { cosf(lat) * cosf(lon), sinf(lat), cosf(lat) * sinf(lon) };
            Vector3 pLat = { cosf(lat + dLat) * cosf(lon), sinf(lat + dLat), cosf(lat + dLat) * sinf(lon) };
            if (Altitud3D(p) < 0.2f) continue;      // solo tierra
            float s = fabsf(Altitud3D(pLat) - Altitud3D(p)) / (dLat * PLANET_R);
            sondas++;
            if (s > maxSlope) { maxSlope = s; slopeEn = lat * RAD2DEG; }
        }
    }
    float angulo = atanf(maxSlope) * RAD2DEG;
    printf("  pendiente max (base 2m, %d sondas): %.3f (%.1f°) a lat %.0f°  →  %s\n",
           sondas, maxSlope, angulo, slopeEn,
           angulo < 45.0f ? "OK caminable" : "ATENCION muy empinado");

    // 5) OCTAVAS FINAS: ¿siguen aportando algo, o se pierden en float32?
    //    Comparamos el relieve con 17 octavas contra el mismo truncado a 12.
    //    Si la diferencia es ~0, las octavas 13-17 son basura y sobran.
    Vector3 probe = { 0.31f, 0.62f, 0.72f };
    Vector3 pn = Vector3Normalize(probe);
    float completo = Altitud3D(pn);
    printf("  altura en sonda:    %+.4f km\n", completo);
    // Diferencia respecto al ruido de gradiente simple (solo 1 octava)
    float solo1 = Noise3D(Vector3Scale(pn, FREC_BASE)) * AMP_BASE * NORMALIZACION;
    printf("  ruido base (1 oct): %+.4f km  →  el fBm lo modula\n", solo1);
    printf("\n");
}

// Barrido de calibración: encuentra (contBias, norm) que dan ~29% de tierra
// y ~9 km de altura máxima, en vez de adivinar a ojo.
static void TestCalibracion(void) {
    printf("── BARRIDO DE CALIBRACION (objetivo: 29%% tierra, 9 km max) ──\n");
    const int NW = 128, NH = 64;
    const int NB = 19;
    float biases[NB], norms[NB], pcts[NB];
    float mejorErr = 1e9f; int mejor = 0;
    for (int b = 0; b < NB; b++) {
        float bias = -0.20f + 0.05f * (float)b;
        int tierra = 0; float maxH = -1e9f;
        for (int y = 0; y < NH; y++) {
            float lat = ((float)y / NH - 0.5f) * PI;
            float cl = cosf(lat), sl = sinf(lat);
            for (int x = 0; x < NW; x++) {
                float lon = ((float)x / NW - 0.5f) * 2.0f * PI;
                float alt = Altitud3DEx({ cl * cosf(lon), sl, cl * sinf(lon) }, bias, 1.0f);
                if (alt > 0.0f) tierra++;
                if (alt > maxH) maxH = alt;
            }
        }
        biases[b] = bias;
        norms[b]  = 9.0f / (maxH > 0.0f ? maxH : 1.0f);   // norm que da 9 km
        pcts[b]   = 100.0f * tierra / (NW * NH);
        float err = fabsf(pcts[b] - 29.0f);
        if (err < mejorErr) { mejorErr = err; mejor = b; }
    }
    for (int b = 0; b < NB; b++)
        printf("   bias %+5.2f → tierra %5.1f%%  normNeeded %.4f%s\n",
               biases[b], pcts[b], norms[b], b == mejor ? "   <-- mejor" : "");
    printf("  MEJOR: CONT_BIAS = %.2f   NORMALIZACION = %.4f  (tierra %.1f%%)\n\n",
           biases[mejor], norms[mejor], pcts[mejor]);
}

int main(void) {
    printf("\n=== TEST RUIDO 3D (gradiente + fBm autoafin) ===\n");
    printf("AMP_BASE=%.1f  AMP_MACRO=%.1f x%d octavas  FBM_GAIN=%.2f  octavas=%d\n",
           AMP_BASE, AMP_MACRO, AMP_MACRO_OCT, FBM_GAIN, FBM_OCTAVES);
    printf("CONT_BIAS=%.2f  NORMALIZACION=%.4f  OCEAN_DEPTH=%.1f\n\n",
           CONT_BIAS, NORMALIZACION, OCEAN_DEPTH);
    TestPerfil();
    TestMapa();
    TestContinuidad();
    TestCalibracion();
    return 0;
}
