#include "clock_screen.h"
#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>

// fuentes
LV_FONT_DECLARE(Minecraft48);
LV_FONT_DECLARE(Minecraft16);
LV_FONT_DECLARE(Minecraft24);
LV_IMG_DECLARE(lumisAssistant);

// ─── PALETA DE COLORES PIXEL ART ─────────────────
#define COLOR_FONDO lv_color_hex(0xF4ECCF)
#define COLOR_BORDE_RETRO lv_color_hex(0x2C2C2C)
#define COLOR_CAJA_HORA lv_color_hex(0xD2A14E)
#define COLOR_TEXTO_DARK lv_color_hex(0x2C2C2C)

static lv_obj_t *label_hora = NULL;
static lv_obj_t *label_fecha = NULL;
static lv_timer_t *timer_reloj = NULL;
static lv_obj_t *panel_musica;
static lv_obj_t *label_modo;
static int modo_actual = 0;
static lv_obj_t *label_tiempo_progreso = NULL;
static int cancion_segundos_actual = 34;
static int cancion_segundos_total = 151;
static lv_obj_t *label_cancion = NULL; // Mapea el texto de la canción en grande
static int indice_cancion_actual = 0;  // Para saber por qué canción va Nuria
static lv_obj_t *img_caratula = NULL;
static lv_obj_t *icono_mp3_global = NULL;

// Variables del Despertador
static lv_obj_t *panel_despertador;
static lv_obj_t *label_alarma_tiempo;
static lv_obj_t *label_info_alarma_principal = NULL; // El texto pequeñito al lado del reloj

// Estas guardan la hora que estás "editando" con los botones + y -
static int edit_hora = 7;
static int edit_min = 30;

// Estas guardan la hora REAL confirmada que hará sonar el despertador
static int alarma_confirmada_hora = 0;
static int alarma_confirmada_min = 0;
static bool alarma_activa = false; // Nos dice si hay una alarma puesta o no

static void cargar_cancion_de_carpeta(void);
static void _actualizar_reloj(lv_timer_t *timer)
{
    (void)timer;
    clock_screen_update();
}

static void cb_toggle_reproductor(lv_event_t *e)
{
    if (lv_obj_has_flag(panel_musica, LV_OBJ_FLAG_HIDDEN))
    {
        lv_obj_clear_flag(panel_musica, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(panel_musica, LV_OBJ_FLAG_HIDDEN);
    }
    // Al final de cb_cambiar_modo añadimos esto:
    indice_cancion_actual = 0; // Reseteamos al cambiar de carpeta
    cargar_cancion_de_carpeta();
}
static void cb_siguiente_cancion(lv_event_t *e)
{
    indice_cancion_actual++;
    cargar_cancion_de_carpeta();
}

static void cb_cambiar_modo(lv_event_t *e)
{
    modo_actual = (modo_actual + 1) % 3;
    if (modo_actual == 0)
        lv_label_set_text(label_modo, "Silencio");
    else if (modo_actual == 1)
        lv_label_set_text(label_modo, "Estudio");
    else if (modo_actual == 2)
        lv_label_set_text(label_modo, "Lata34");

    // ── AÑADE ESTAS DOS LÍNEAS JUSTO AQUÍ ABAJO ──
    indice_cancion_actual = 0;   // Forzamos a que empiece en la primera canción de la nueva lista
    cargar_cancion_de_carpeta(); // Forzamos a que lea la carpeta inmediatamente al cambiar
}

static void cb_toggle_despertador(lv_event_t *e)
{
    if (lv_obj_has_flag(panel_despertador, LV_OBJ_FLAG_HIDDEN))
    {
        // Al abrir el panel, cargamos el último valor editado
        lv_obj_clear_flag(panel_despertador, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(panel_despertador, LV_OBJ_FLAG_HIDDEN);
    }
}

static void _actualizar_texto_alarma_temporal(void)
{
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", edit_hora, edit_min);
    lv_label_set_text(label_alarma_tiempo, buf);
}

static void cb_modificar_alarma(lv_event_t *e)
{
    long accion = (long)lv_event_get_user_data(e);

    if (accion == 1)
        edit_hora = (edit_hora + 1) % 24;
    else if (accion == 2)
        edit_hora = (edit_hora - 1 + 24) % 24;
    else if (accion == 3)
        edit_min = (edit_min + 5) % 60;
    else if (accion == 4)
        edit_min = (edit_min - 5 + 60) % 60;

    _actualizar_texto_alarma_temporal();
}

// ── NÚCLEO: EVENTO DE CONFIRMACIÓN ──
static void cb_confirmar_alarma(lv_event_t *e)
{
    // 1. Traspasamos la hora editada a la alarma real del sistema
    alarma_confirmada_hora = edit_hora;
    alarma_confirmada_min = edit_min;
    alarma_activa = true;

    // 2. Pintamos el texto pequeño al lado del reloj principal
    char buf[20];
    snprintf(buf, sizeof(buf), "ALARM: %02d:%02d", alarma_confirmada_hora, alarma_confirmada_min);
    lv_label_set_text(label_info_alarma_principal, buf);

    // 3. Ocultamos el panel
    lv_obj_add_flag(panel_despertador, LV_OBJ_FLAG_HIDDEN);
    printf("[SISTEMA] Alarma fijada con éxito a las %02d:%02d\n", alarma_confirmada_hora, alarma_confirmada_min);
}
static void cb_borrar_alarma(lv_event_t *e)
{
    // 1. Apagamos el interruptor de la alarma
    alarma_activa = false;

    // 2. Volvemos a poner el texto pequeñito de la pantalla principal en OFF
    lv_label_set_text(label_info_alarma_principal, "ALARM: OFF");

    // 3. Cerramos el panel para volver a la pantalla principal
    lv_obj_add_flag(panel_despertador, LV_OBJ_FLAG_HIDDEN);

    printf("[SISTEMA] Alarma borrada por el usuario.\n");
}

#include <string.h>

// Función para obtener la duración real de un MP3 leyendo sus bytes de cabecera
static int obtener_duracion_mp3(const char *ruta_archivo)
{
    FILE *f = fopen(ruta_archivo, "rb");
    if (!f)
        return 180;

    // 1. Saltamos el tag ID3v2
    long offset_audio = 0;
    unsigned char cabecera[10];
    if (fread(cabecera, 1, 10, f) == 10)
    {
        if (cabecera[0] == 'I' && cabecera[1] == 'D' && cabecera[2] == '3')
        {
            long tag_size = ((cabecera[6] & 0x7F) << 21) |
                            ((cabecera[7] & 0x7F) << 14) |
                            ((cabecera[8] & 0x7F) << 7) |
                            (cabecera[9] & 0x7F);
            offset_audio = 10 + tag_size;
        }
    }

    static const int tabla_bitrates[] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};
    static const int tabla_samplerate[] = {44100, 48000, 32000, 0};

    // 2. Leemos el primer frame MPEG
    fseek(f, offset_audio, SEEK_SET);
    unsigned char frame[4];
    long pos = offset_audio;
    int encontrado = 0;
    int samplerate = 44100;
    int canales = 2;

    for (int i = 0; i < 32768; i++)
    {
        fseek(f, pos, SEEK_SET);
        if (fread(frame, 1, 4, f) < 4)
            break;

        if (frame[0] == 0xFF && (frame[1] & 0xE0) == 0xE0)
        {
            int version = (frame[1] >> 3) & 0x03;
            int layer = (frame[1] >> 1) & 0x03;
            int idx_sr = (frame[2] >> 2) & 0x03;
            int modo = (frame[3] >> 6) & 0x03; // 3 = mono

            if (version == 3 && layer == 1 && idx_sr < 3)
            {
                samplerate = tabla_samplerate[idx_sr];
                canales = (modo == 3) ? 1 : 2;
                encontrado = 1;
                break;
            }
        }
        pos++;
    }

    if (!encontrado)
    {
        fclose(f);
        return 180;
    }

    // 3. Buscamos el header Xing/Info dentro del primer frame
    //    Offset desde inicio del frame: 4 bytes header + side_info
    //    Side info: 32 bytes stereo, 17 bytes mono (MPEG1)
    int side_info_size = (canales == 1) ? 17 : 32;
    long xing_offset = pos + 4 + side_info_size;

    fseek(f, xing_offset, SEEK_SET);
    unsigned char xing_buf[120];
    int xing_leidos = (int)fread(xing_buf, 1, sizeof(xing_buf), f);

    // Buscamos "Xing" o "Info" en los primeros bytes
    int xing_pos = -1;
    for (int i = 0; i <= xing_leidos - 4; i++)
    {
        if ((memcmp(xing_buf + i, "Xing", 4) == 0) ||
            (memcmp(xing_buf + i, "Info", 4) == 0))
        {
            xing_pos = i;
            printf("[DURACION] Header %c%c%c%c encontrado!\n",
                   xing_buf[i], xing_buf[i + 1], xing_buf[i + 2], xing_buf[i + 3]);
            break;
        }
    }

    if (xing_pos >= 0 && xing_pos + 8 <= xing_leidos)
    {
        unsigned char *x = xing_buf + xing_pos;
        unsigned long flags = ((unsigned long)x[4] << 24) | ((unsigned long)x[5] << 16) |
                              ((unsigned long)x[6] << 8) | (unsigned long)x[7];

        if (flags & 0x01) // Bit 0 = tiene número de frames
        {
            unsigned long num_frames = ((unsigned long)x[8] << 24) |
                                       ((unsigned long)x[9] << 16) |
                                       ((unsigned long)x[10] << 8) |
                                       (unsigned long)x[11];

            // 1152 muestras por frame en MPEG1 Layer III
            int duracion = (int)((double)num_frames * 1152.0 / samplerate);
            printf("[DURACION] Xing: %lu frames → %d seg (%d:%02d)\n",
                   num_frames, duracion, duracion / 60, duracion % 60);
            fclose(f);
            return duracion > 0 ? duracion : 180;
        }
    }

    // 4. Fallback: si no hay Xing, usamos tamaño + bitrate del frame
    printf("[DURACION] Sin Xing, calculando por tamaño...\n");
    int idx_br = (frame[2] >> 4) & 0x0F;
    int bitrate_kbps = tabla_bitrates[idx_br];

    fseek(f, 0, SEEK_END);
    long tamano_audio = ftell(f) - offset_audio;
    fclose(f);

    if (bitrate_kbps >= 32 && bitrate_kbps <= 320)
    {
        int duracion = (int)((tamano_audio * 8L) / ((long)bitrate_kbps * 1000L));
        printf("[DURACION] Bitrate frame: %d kbps → %d seg (%d:%02d)\n",
               bitrate_kbps, duracion, duracion / 60, duracion % 60);
        return duracion > 0 ? duracion : 180;
    }

    return (int)(tamano_audio / 24000); // último fallback 192kbps
}
// Función PRO para extraer el JPG oculto dentro del archivo MP3
// ─── EXTRAE LA CARÁTULA (APIC) DE UN ID3v2 ─────────────────────────────────
// Devuelve 1 si tuvo éxito, 0 si no hay carátula.
static int extraer_caratula_mp3(const char *ruta_mp3, const char *ruta_salida_jpg)
{
    printf("[DEBUG] Abriendo: %s\n", ruta_mp3);
    FILE *f = fopen(ruta_mp3, "rb");
    if (!f)
    {
        printf("[DEBUG] ERROR: No pude abrir el archivo\n");
        return 0;
    }

    unsigned char header[10];
    if (fread(header, 1, 10, f) < 10)
    {
        fclose(f);
        printf("[DEBUG] ERROR: No pude leer 10 bytes de cabecera\n");
        return 0;
    }

    printf("[DEBUG] Primeros 3 bytes: %c%c%c (version %d.%d)\n",
           header[0], header[1], header[2], header[3], header[4]);

    if (header[0] != 'I' || header[1] != 'D' || header[2] != '3')
    {
        // ── Puede que el MP3 NO tenga ID3v2 al principio.
        // Algunos editores ponen el tag ID3v2 AL FINAL del archivo.
        printf("[DEBUG] No hay ID3v2 al principio. Buscando al final...\n");

        fseek(f, -128, SEEK_END);
        unsigned char id3v1[3];
        fread(id3v1, 1, 3, f);
        if (id3v1[0] == 'T' && id3v1[1] == 'A' && id3v1[2] == 'G')
            printf("[DEBUG] Tiene ID3v1 al final (no contiene carátula)\n");
        else
            printf("[DEBUG] Sin ningún tag ID3 reconocido\n");

        fclose(f);
        return 0;
    }

    int version_mayor = header[3];
    long tag_size = ((header[6] & 0x7F) << 21) |
                    ((header[7] & 0x7F) << 14) |
                    ((header[8] & 0x7F) << 7) |
                    (header[9] & 0x7F);

    printf("[DEBUG] ID3v2.%d detectado. Tamaño del tag: %ld bytes\n", version_mayor, tag_size);

    unsigned char *tag_data = (unsigned char *)malloc(tag_size);
    if (!tag_data)
    {
        fclose(f);
        printf("[DEBUG] ERROR: malloc falló\n");
        return 0;
    }

    long leidos = (long)fread(tag_data, 1, tag_size, f);
    fclose(f);
    printf("[DEBUG] Bytes leídos del tag: %ld\n", leidos);

    // ── Listamos TODOS los frames encontrados ──────────────────────────────
    long pos = 0;
    int encontrado = 0;

    printf("[DEBUG] Frames encontrados:\n");
    while (pos < tag_size - 10)
    {
        char frame_id[5];
        memcpy(frame_id, tag_data + pos, 4);
        frame_id[4] = '\0';

        // Si el frame_id empieza por \0 es padding, fin del tag
        if (frame_id[0] == '\0')
            break;

        long frame_size;
        if (version_mayor == 4)
            frame_size = ((tag_data[pos + 4] & 0x7F) << 21) |
                         ((tag_data[pos + 5] & 0x7F) << 14) |
                         ((tag_data[pos + 6] & 0x7F) << 7) |
                         (tag_data[pos + 7] & 0x7F);
        else
            frame_size = ((long)tag_data[pos + 4] << 24) |
                         ((long)tag_data[pos + 5] << 16) |
                         ((long)tag_data[pos + 6] << 8) |
                         (long)tag_data[pos + 7];

        printf("[DEBUG]   Frame: '%s'  tamaño: %ld\n", frame_id, frame_size);

        if (frame_size <= 0 || pos + 10 + frame_size > tag_size)
            break;

        if (strcmp(frame_id, "APIC") == 0)
        {
            unsigned char *apic = tag_data + pos + 10;
            long apic_size = frame_size;

            unsigned char encoding = apic[0];
            printf("[DEBUG] APIC encontrado! Encoding: %d\n", encoding);

            long offset = 1;

            // Leemos MIME type para saber qué formato es
            char mime[64] = "";
            int mi = 0;
            while (offset < apic_size && apic[offset] != '\0' && mi < 63)
                mime[mi++] = apic[offset++];
            mime[mi] = '\0';
            offset++; // \0 del mime

            unsigned char pic_type = apic[offset++];
            printf("[DEBUG] MIME: '%s'  Tipo imagen: %d\n", mime, pic_type);

            // Saltamos descripción (respetando encoding UTF-16 con \0\0)
            if (encoding == 1 || encoding == 2) // UTF-16
            {
                while (offset < apic_size - 1 &&
                       !(apic[offset] == 0x00 && apic[offset + 1] == 0x00))
                    offset += 2;
                offset += 2;
            }
            else // Latin-1 o UTF-8
            {
                while (offset < apic_size && apic[offset] != '\0')
                    offset++;
                offset++;
            }

            long imagen_size = apic_size - offset;
            printf("[DEBUG] Offset imagen: %ld  Tamaño imagen: %ld bytes\n", offset, imagen_size);
            printf("[DEBUG] Primeros bytes imagen: %02X %02X %02X %02X\n",
                   apic[offset], apic[offset + 1], apic[offset + 2], apic[offset + 3]);

            if (imagen_size > 0)
            {
                printf("[DEBUG] Escribiendo en: %s\n", ruta_salida_jpg);
                FILE *out = fopen(ruta_salida_jpg, "wb");
                if (!out)
                {
                    printf("[DEBUG] ERROR: No pude crear el archivo de salida\n");
                }
                else
                {
                    fwrite(apic + offset, 1, imagen_size, out);
                    fclose(out);
                    printf("[DEBUG] ¡Carátula guardada OK! (%ld bytes)\n", imagen_size);
                    encontrado = 1;
                }
            }
            break;
        }

        pos += 10 + frame_size;
    }

    if (!encontrado)
        printf("[DEBUG] No se encontró frame APIC en el tag\n");

    free(tag_data);
    return encontrado;
}

static void cargar_cancion_de_carpeta(void)
{
    if (label_cancion == NULL)
        return;

    // 1. Decidimos qué carpeta mirar según el botón "Cambiar Lista"
    const char *ruta_carpeta = "../src/tarjeta_sd/Silencio";
    if (modo_actual == 1)
        ruta_carpeta = "../src/tarjeta_sd/Estudio";
    else if (modo_actual == 2)
        ruta_carpeta = "../src/tarjeta_sd/Lata34";

    DIR *dir = opendir(ruta_carpeta);
    if (dir == NULL)
    {
        // ── AÑADE ESTAS DOS LÍNEAS DE CONTROL AQUÍ ──
        printf("[SD] ERROR al abrir la carpeta: %s\n", ruta_carpeta);
        perror("[SD] Motivo del fallo");

        lv_label_set_text(label_cancion, "Sin Tarjeta SD");
        return;
    }

    struct dirent *entrada;
    int contador = 0;
    char nombre_encontrado[64] = "";

    // 2. Escaneamos los archivos de la carpeta (.mp3, etc.)
    while ((entrada = readdir(dir)) != NULL)
    {
        // Ignoramos archivos invisibles del sistema operativo
        if (entrada->d_name[0] == '.')
            continue;

        if (contador == indice_cancion_actual)
        {
            snprintf(nombre_encontrado, sizeof(nombre_encontrado), "%s", entrada->d_name);
            break;
        }
        contador++;
    }

    // Si Nuria llega al final de las canciones de la carpeta, vuelve a la primera
    if (nombre_encontrado[0] == '\0' && indice_cancion_actual > 0)
    {
        indice_cancion_actual = 0;
        rewinddir(dir);
        while ((entrada = readdir(dir)) != NULL)
        {
            if (entrada->d_name[0] == '.')
                continue;
            snprintf(nombre_encontrado, sizeof(nombre_encontrado), "%s", entrada->d_name);
            break;
        }
    }
    // Dentro de cargar_cancion_de_carpeta, donde actualizas la pantalla:
    if (nombre_encontrado[0] != '\0')
    {
        lv_label_set_text(label_cancion, nombre_encontrado);

        // ── AQUÍ CALCULAMOS LA DURACIÓN REAL AUTOMÁTICAMENTE ──
        char ruta_completa[256];
        snprintf(ruta_completa, sizeof(ruta_completa), "%s/%s", ruta_carpeta, nombre_encontrado);
        cancion_segundos_total = obtener_duracion_mp3(ruta_completa);
    }
    closedir(dir);

    // 3. ¡Actualizamos la pantalla con el archivo real!
    // ── Justo antes del bloque "MAGIA: EXTRAER LA CARÁTULA" ─────────────
    if (nombre_encontrado[0] == '\0')
    {
        lv_obj_clear_flag(icono_mp3_global, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(img_caratula, LV_OBJ_FLAG_HIDDEN);
        return; // ← carpeta vacía, no intentamos extraer nada
    }

    // ── MAGIA: EXTRAER LA CARÁTULA INCRUSTADA EN TIEMPO REAL ──
    char ruta_temp_jpg[256] = "../src/tarjeta_sd/cover_temp.jpg";
    char ruta_lvgl_jpg[256] = "A:../src/tarjeta_sd/cover_temp.jpg";

    // Extraemos la carátula desde la ruta_completa calculada antes para la duración
    char ruta_completa[256];
    snprintf(ruta_completa, sizeof(ruta_completa), "%s/%s", ruta_carpeta, nombre_encontrado);

    if (extraer_caratula_mp3(ruta_completa, ruta_temp_jpg))
    {
        // ¡Tenemos foto! Ocultamos la nota musical y encendemos el lienzo
        lv_obj_add_flag(icono_mp3_global, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(img_caratula, LV_OBJ_FLAG_HIDDEN);

        // Vaciamos la memoria caché de LVGL para obligarle a cargar la nueva foto
        // lv_image_cache_drop(NULL);

        // Le pasamos el JPG puro recién extraído al simulador
        lv_img_set_src(img_caratula, ruta_lvgl_jpg);
    }
    else
    {
        // El MP3 no tiene carátula incrustada, mostramos la nota musical retro
        lv_obj_clear_flag(icono_mp3_global, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(img_caratula, LV_OBJ_FLAG_HIDDEN);
    }
    // Cada vez que cambia de canción, el segundero de Nuria vuelve a empezar en 0:00
    cancion_segundos_actual = 0;
}

void clock_screen_create(void)
{
    lv_obj_t *pantalla = lv_scr_act();
    lv_obj_set_style_bg_color(pantalla, COLOR_FONDO, 0);
    lv_obj_set_style_bg_opa(pantalla, LV_OPA_COVER, 0);
    lv_obj_clear_flag(pantalla, LV_OBJ_FLAG_SCROLLABLE);

    // ── CAJA DE LA HORA ESTILO PIXEL ART ──
    lv_obj_t *bloque_hora = lv_obj_create(pantalla);
    lv_obj_set_size(bloque_hora, 280, 110);
    lv_obj_set_pos(bloque_hora, 40, 40);
    lv_obj_set_style_bg_color(bloque_hora, COLOR_CAJA_HORA, 0);
    lv_obj_set_style_radius(bloque_hora, 0, 0);
    lv_obj_set_style_border_width(bloque_hora, 5, 0);
    lv_obj_set_style_border_color(bloque_hora, COLOR_BORDE_RETRO, 0);
    lv_obj_set_style_pad_all(bloque_hora, 0, 0);

    label_hora = lv_label_create(bloque_hora);
    lv_obj_set_style_text_color(label_hora, COLOR_TEXTO_DARK, 0);
    lv_obj_set_style_text_font(label_hora, &Minecraft48, 0);
    lv_label_set_text(label_hora, "00:00");
    lv_obj_align(label_hora, LV_ALIGN_CENTER, 0, 0);

    // ── 🔔 TEXTO ALARMA PEQUEÑA (Al lado del reloj) ──
    label_info_alarma_principal = lv_label_create(pantalla);
    lv_obj_set_pos(label_info_alarma_principal, 340, 80);                                // Situado justo a la derecha de la caja dorada
    lv_obj_set_style_text_color(label_info_alarma_principal, lv_color_hex(0xA3423C), 0); // Un rojo oscuro retro elegante
    lv_obj_set_style_text_font(label_info_alarma_principal, &Minecraft24, 0);            // Texto mediano legible
    lv_label_set_text(label_info_alarma_principal, "ALARM: OFF");                        // Por defecto apagada

    label_fecha = lv_label_create(pantalla);
    lv_obj_set_style_text_color(label_fecha, COLOR_TEXTO_DARK, 0);
    lv_obj_set_style_text_font(label_fecha, &Minecraft48, 0);
    lv_obj_set_pos(label_fecha, 40, 170);

    // ── 🤖 ROBOT ASISTENTE ──
    lv_obj_t *contenedor_robot = lv_obj_create(pantalla);
    lv_obj_set_size(contenedor_robot, 300, 300);
    lv_obj_set_pos(contenedor_robot, 650, 250);
    lv_obj_set_style_bg_color(contenedor_robot, COLOR_FONDO, 0);
    lv_obj_set_style_border_width(contenedor_robot, 0, 0);

    lv_obj_t *img_robot = lv_img_create(pantalla);
    lv_img_set_src(img_robot, &lumisAssistant);
    lv_obj_set_pos(img_robot, 680, 230);

    // ── 💬 BOCADILLO DE TEXTO ──
    lv_obj_t *bocadillo = lv_obj_create(pantalla);
    lv_obj_set_size(bocadillo, 640, 130);
    lv_obj_set_pos(bocadillo, 40, 260);
    lv_obj_set_style_bg_color(bocadillo, lv_color_white(), 0);
    lv_obj_set_style_radius(bocadillo, 16, 0);
    lv_obj_set_style_border_width(bocadillo, 4, 0);
    lv_obj_set_style_border_color(bocadillo, COLOR_BORDE_RETRO, 0);
    lv_obj_set_style_pad_all(bocadillo, 20, 0);
    lv_obj_clear_flag(bocadillo, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label_frase = lv_label_create(bocadillo);
    lv_obj_set_style_text_font(label_frase, &Minecraft24, 0);
    lv_obj_set_style_text_color(label_frase, COLOR_TEXTO_DARK, 0);
    lv_label_set_text(label_frase, "Hola Hugo! Esto es una frase de prueba para\nver como se lee el dialogo curvo en pantalla.");
    lv_obj_align(label_frase, LV_ALIGN_LEFT_MID, 10, 0);

    timer_reloj = lv_timer_create(_actualizar_reloj, 1000, NULL);

    // Botón Música (M)
    lv_obj_t *btn_nota = lv_button_create(pantalla);
    lv_obj_set_size(btn_nota, 60, 60);
    lv_obj_set_pos(btn_nota, 40, 500);
    lv_obj_set_style_bg_color(btn_nota, lv_color_hex(0xD4A373), 0);
    lv_obj_set_style_radius(btn_nota, 0, 0);
    lv_obj_set_style_border_width(btn_nota, 4, 0);
    lv_obj_set_style_border_color(btn_nota, COLOR_BORDE_RETRO, 0);
    lv_obj_add_event_cb(btn_nota, cb_toggle_reproductor, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_nota = lv_label_create(btn_nota);
    lv_label_set_text(label_nota, "M");
    lv_obj_set_style_text_font(label_nota, &Minecraft24, 0);
    lv_obj_align(label_nota, LV_ALIGN_CENTER, 0, 0);

    // Botón Despertador (A)
    lv_obj_t *btn_campana = lv_button_create(pantalla);
    lv_obj_set_size(btn_campana, 60, 60);
    lv_obj_set_pos(btn_campana, 120, 500);
    lv_obj_set_style_bg_color(btn_campana, lv_color_hex(0xD4A373), 0);
    lv_obj_set_style_radius(btn_campana, 0, 0);
    lv_obj_set_style_border_width(btn_campana, 4, 0);
    lv_obj_set_style_border_color(btn_campana, COLOR_BORDE_RETRO, 0);
    lv_obj_add_event_cb(btn_campana, cb_toggle_despertador, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_campana = lv_label_create(btn_campana);
    lv_label_set_text(label_campana, "A");
    lv_obj_set_style_text_font(label_campana, &Minecraft24, 0);
    lv_obj_align(label_campana, LV_ALIGN_CENTER, 0, 0);

    // ── PANEL REPRODUCTOR MULTIMEDIA ──
    // ── PANEL REPRODUCTOR MULTIMEDIA ──
    panel_musica = lv_obj_create(pantalla);
    lv_obj_set_size(panel_musica, 944, 520);
    lv_obj_set_pos(panel_musica, 40, 40);
    lv_obj_set_style_bg_color(panel_musica, lv_color_white(), 0);
    lv_obj_set_style_radius(panel_musica, 24, 0);
    lv_obj_set_style_border_width(panel_musica, 6, 0);
    lv_obj_set_style_border_color(panel_musica, COLOR_BORDE_RETRO, 0);
    lv_obj_add_flag(panel_musica, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(panel_musica, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label_titulo = lv_label_create(panel_musica);
    lv_label_set_text(label_titulo, "REPRODUCTOR MULTIMEDIA");
    lv_obj_set_style_text_font(label_titulo, &Minecraft24, 0);
    lv_obj_align(label_titulo, LV_ALIGN_TOP_MID, 0, 20);

    // 📸 FOTO/CARÁTULA DEL MP3 (Estilo Caja Pixel-Art a la izquierda)
    lv_obj_t *caja_foto_mp3 = lv_obj_create(panel_musica);
    lv_obj_set_size(caja_foto_mp3, 180, 180);
    lv_obj_align(caja_foto_mp3, LV_ALIGN_CENTER, -240, -40);             // Súper a la izquierda
    lv_obj_set_style_bg_color(caja_foto_mp3, lv_color_hex(0x2C2C2C), 0); // Fondo oscuro retro
    lv_obj_set_style_radius(caja_foto_mp3, 12, 0);
    lv_obj_set_style_border_width(caja_foto_mp3, 4, 0);
    lv_obj_set_style_border_color(caja_foto_mp3, COLOR_CAJA_HORA, 0); // Borde dorado

    // Icono musical dentro de la foto
    // 1. Icono musical dentro de la foto (usando la variable global)
    icono_mp3_global = lv_label_create(caja_foto_mp3);
    lv_label_set_text(icono_mp3_global, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(icono_mp3_global, &Minecraft48, 0);
    lv_obj_set_style_text_color(icono_mp3_global, lv_color_white(), 0);
    lv_obj_align(icono_mp3_global, LV_ALIGN_CENTER, 0, 0);

    // 2. 🖼️ Lienzo invisible para la foto real (usando la variable global)
    img_caratula = lv_img_create(caja_foto_mp3);
    lv_obj_align(img_caratula, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(img_caratula, LV_OBJ_FLAG_HIDDEN); // Se queda oculta hasta que haya foto
    // TEXTOS DESPLAZADOS A LA DERECHA PARA DEJAR SITIO A LA FOTO
    lv_obj_t *label_now_playing = lv_label_create(panel_musica);
    lv_label_set_text(label_now_playing, "NOW PLAYING:");
    lv_obj_set_style_text_font(label_now_playing, &Minecraft16, 0);
    lv_obj_align(label_now_playing, LV_ALIGN_CENTER, 80, -110);

    label_cancion = lv_label_create(panel_musica);
    lv_label_set_text(label_cancion, "Intro (Lata34 Mix)");
    lv_obj_set_style_text_font(label_cancion, &Minecraft24, 0); // 1. Letra más pequeña (24 en vez de 48)

    // 2. Le ponemos un límite de ancho y hacemos que ruede si se pasa
    lv_obj_set_width(label_cancion, 450);
    lv_label_set_long_mode(label_cancion, LV_LABEL_LONG_SCROLL_CIRCULAR);

    lv_obj_align(label_cancion, LV_ALIGN_CENTER, 80, -60);

    lv_obj_t *label_autor = lv_label_create(panel_musica);
    lv_label_set_text(label_autor, "Exclusivo para Nuria "); // ¡Corregido para Nuria con un corazón!
    lv_obj_set_style_text_font(label_autor, &Minecraft24, 0);
    lv_obj_set_style_text_color(label_autor, lv_color_hex(0xA3423C), 0); // Rojo elegante
    lv_obj_align(label_autor, LV_ALIGN_CENTER, 80, -10);

    // ⏱️ MARCADOR DE TIEMPO (0:34 - 2:31)
    label_tiempo_progreso = lv_label_create(panel_musica);
    lv_label_set_text(label_tiempo_progreso, "0:34 - 2:31");
    lv_obj_set_style_text_font(label_tiempo_progreso, &Minecraft24, 0);
    lv_obj_set_style_text_color(label_tiempo_progreso, lv_color_hex(0x666666), 0);
    lv_obj_align(label_tiempo_progreso, LV_ALIGN_CENTER, 80, 30); // Justo debajo del autor

    label_modo = lv_label_create(panel_musica);
    lv_label_set_text(label_modo, "Silencio");
    lv_obj_set_style_text_font(label_modo, &Minecraft24, 0);
    lv_obj_set_style_text_color(label_modo, lv_color_hex(0xD2A14E), 0);
    lv_obj_align(label_modo, LV_ALIGN_CENTER, 80, 70);
    lv_obj_t *btn_modo = lv_button_create(panel_musica);
    lv_obj_set_size(btn_modo, 220, 80);
    lv_obj_align(btn_modo, LV_ALIGN_BOTTOM_LEFT, 60, -40);
    lv_obj_set_style_bg_color(btn_modo, lv_color_hex(0xE9EDC9), 0);
    lv_obj_set_style_radius(btn_modo, 0, 0);
    lv_obj_set_style_border_width(btn_modo, 4, 0);
    lv_obj_set_style_border_color(btn_modo, COLOR_BORDE_RETRO, 0);
    lv_obj_add_event_cb(btn_modo, cb_cambiar_modo, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_btn_modo = lv_label_create(btn_modo);
    lv_label_set_text(label_btn_modo, "CAMBIAR\nLISTA");
    lv_obj_set_style_text_font(label_btn_modo, &Minecraft16, 0);
    lv_obj_align(label_btn_modo, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *btn_play = lv_button_create(panel_musica);
    lv_obj_set_size(btn_play, 160, 80);
    lv_obj_align_to(btn_play, btn_modo, LV_ALIGN_OUT_RIGHT_MID, 40, 0);
    lv_obj_set_style_bg_color(btn_play, lv_color_hex(0xCCD5AE), 0);
    lv_obj_set_style_radius(btn_play, 0, 0);
    lv_obj_set_style_border_width(btn_play, 4, 0);
    lv_obj_set_style_border_color(btn_play, COLOR_BORDE_RETRO, 0);

    lv_obj_t *label_play = lv_label_create(btn_play);
    lv_label_set_text(label_play, "PLAY");
    lv_obj_set_style_text_font(label_play, &Minecraft24, 0);
    lv_obj_align(label_play, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *btn_next = lv_button_create(panel_musica);
    lv_obj_set_size(btn_next, 160, 80);
    lv_obj_align_to(btn_next, btn_play, LV_ALIGN_OUT_RIGHT_MID, 40, 0);
    lv_obj_set_style_bg_color(btn_next, lv_color_hex(0xCCD5AE), 0);
    lv_obj_set_style_radius(btn_next, 0, 0);
    lv_obj_set_style_border_width(btn_next, 4, 0);
    lv_obj_set_style_border_color(btn_next, COLOR_BORDE_RETRO, 0);
    // Busca btn_next y añade esta línea abajo:
    lv_obj_add_event_cb(btn_next, cb_siguiente_cancion, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_next = lv_label_create(btn_next);
    lv_label_set_text(label_next, ">>");
    lv_obj_set_style_text_font(label_next, &Minecraft24, 0);
    lv_obj_align(label_next, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *btn_cerrar = lv_button_create(panel_musica);
    lv_obj_set_size(btn_cerrar, 140, 80);
    lv_obj_align(btn_cerrar, LV_ALIGN_BOTTOM_RIGHT, -60, -40);
    lv_obj_set_style_bg_color(btn_cerrar, lv_color_hex(0xE63946), 0);
    lv_obj_set_style_radius(btn_cerrar, 0, 0);
    lv_obj_set_style_border_width(btn_cerrar, 4, 0);
    lv_obj_set_style_border_color(btn_cerrar, COLOR_BORDE_RETRO, 0);
    lv_obj_add_event_cb(btn_cerrar, cb_toggle_reproductor, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_cerrar = lv_label_create(btn_cerrar);
    lv_label_set_text(label_cerrar, "VOLVER");
    lv_obj_set_style_text_font(label_cerrar, &Minecraft16, 0);
    lv_obj_set_style_text_color(label_cerrar, lv_color_white(), 0);
    lv_obj_align(label_cerrar, LV_ALIGN_CENTER, 0, 0);

    // ── PANEL CONFIGURACIÓN DESPERTADOR ──
    panel_despertador = lv_obj_create(pantalla);
    lv_obj_set_size(panel_despertador, 944, 520);
    lv_obj_set_pos(panel_despertador, 40, 40);
    lv_obj_set_style_bg_color(panel_despertador, lv_color_white(), 0);
    lv_obj_set_style_radius(panel_despertador, 24, 0);
    lv_obj_set_style_border_width(panel_despertador, 6, 0);
    lv_obj_set_style_border_color(panel_despertador, COLOR_BORDE_RETRO, 0);
    lv_obj_add_flag(panel_despertador, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(panel_despertador, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label_titulo_desp = lv_label_create(panel_despertador);
    lv_label_set_text(label_titulo_desp, "CONFIGURACION DE ALARMA");
    lv_obj_set_style_text_font(label_titulo_desp, &Minecraft24, 0);
    lv_obj_align(label_titulo_desp, LV_ALIGN_TOP_MID, 0, 20);

    label_alarma_tiempo = lv_label_create(panel_despertador);
    lv_label_set_text(label_alarma_tiempo, "07:30");
    lv_obj_set_style_text_font(label_alarma_tiempo, &Minecraft48, 0);
    lv_obj_align(label_alarma_tiempo, LV_ALIGN_CENTER, 0, -40);

    // Botones + y -
    lv_obj_t *btn_hora_up = lv_button_create(panel_despertador);
    lv_obj_set_size(btn_hora_up, 80, 50);
    lv_obj_align(btn_hora_up, LV_ALIGN_CENTER, -60, -110);
    lv_obj_set_style_bg_color(btn_hora_up, lv_color_hex(0xE9EDC9), 0);
    lv_obj_set_style_border_width(btn_hora_up, 3, 0);
    lv_obj_set_style_border_color(btn_hora_up, COLOR_BORDE_RETRO, 0);
    lv_obj_add_event_cb(btn_hora_up, cb_modificar_alarma, LV_EVENT_CLICKED, (void *)1);

    lv_obj_t *label_h_up = lv_label_create(btn_hora_up);
    lv_label_set_text(label_h_up, "+");
    lv_obj_set_style_text_font(label_h_up, &Minecraft24, 0);
    lv_obj_align(label_h_up, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *btn_hora_down = lv_button_create(panel_despertador);
    lv_obj_set_size(btn_hora_down, 80, 50);
    lv_obj_align(btn_hora_down, LV_ALIGN_CENTER, -60, 30);
    lv_obj_set_style_bg_color(btn_hora_down, lv_color_hex(0xE9EDC9), 0);
    lv_obj_set_style_border_width(btn_hora_down, 3, 0);
    lv_obj_set_style_border_color(btn_hora_down, COLOR_BORDE_RETRO, 0);
    lv_obj_add_event_cb(btn_hora_down, cb_modificar_alarma, LV_EVENT_CLICKED, (void *)2);

    lv_obj_t *label_h_down = lv_label_create(btn_hora_down);
    lv_label_set_text(label_h_down, "-");
    lv_obj_set_style_text_font(label_h_down, &Minecraft24, 0);
    lv_obj_align(label_h_down, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *btn_min_up = lv_button_create(panel_despertador);
    lv_obj_set_size(btn_min_up, 80, 50);
    lv_obj_align(btn_min_up, LV_ALIGN_CENTER, 60, -110);
    lv_obj_set_style_bg_color(btn_min_up, lv_color_hex(0xE9EDC9), 0);
    lv_obj_set_style_border_width(btn_min_up, 3, 0);
    lv_obj_set_style_border_color(btn_min_up, COLOR_BORDE_RETRO, 0);
    lv_obj_add_event_cb(btn_min_up, cb_modificar_alarma, LV_EVENT_CLICKED, (void *)3);

    lv_obj_t *label_m_up = lv_label_create(btn_min_up);
    lv_label_set_text(label_m_up, "+");
    lv_obj_set_style_text_font(label_m_up, &Minecraft24, 0);
    lv_obj_align(label_m_up, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *btn_min_down = lv_button_create(panel_despertador);
    lv_obj_set_size(btn_min_down, 80, 50);
    lv_obj_align(btn_min_down, LV_ALIGN_CENTER, 60, 30);
    lv_obj_set_style_bg_color(btn_min_down, lv_color_hex(0xE9EDC9), 0);
    lv_obj_set_style_border_width(btn_min_down, 3, 0);
    lv_obj_set_style_border_color(btn_min_down, COLOR_BORDE_RETRO, 0);
    lv_obj_add_event_cb(btn_min_down, cb_modificar_alarma, LV_EVENT_CLICKED, (void *)4);

    lv_obj_t *label_m_down = lv_label_create(btn_min_down);
    lv_label_set_text(label_m_down, "-");
    lv_obj_set_style_text_font(label_m_down, &Minecraft24, 0);
    lv_obj_align(label_m_down, LV_ALIGN_CENTER, 0, 0);

    // 🟢 BOTÓN NUEVO: CONFIRMAR / ACEPTAR (Abajo a la derecha, Verde)
    lv_obj_t *btn_aceptar = lv_button_create(panel_despertador);
    lv_obj_set_size(btn_aceptar, 180, 80);
    lv_obj_align(btn_aceptar, LV_ALIGN_BOTTOM_RIGHT, -60, -40);
    lv_obj_set_style_bg_color(btn_aceptar, lv_color_hex(0xCCD5AE), 0); // Verde suave retro
    lv_obj_set_style_radius(btn_aceptar, 0, 0);
    lv_obj_set_style_border_width(btn_aceptar, 4, 0);
    lv_obj_set_style_border_color(btn_aceptar, COLOR_BORDE_RETRO, 0);
    lv_obj_add_event_cb(btn_aceptar, cb_confirmar_alarma, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_aceptar = lv_label_create(btn_aceptar);
    lv_label_set_text(label_aceptar, "ACEPTAR");
    lv_obj_set_style_text_font(label_aceptar, &Minecraft24, 0);
    lv_obj_set_style_text_color(label_aceptar, COLOR_TEXTO_DARK, 0);
    lv_obj_align(label_aceptar, LV_ALIGN_CENTER, 0, 0);

    // 🔴 BOTÓN MODIFICADO: BORRAR ALARMA (Rojo)
    lv_obj_t *btn_cancelar = lv_button_create(panel_despertador);
    lv_obj_set_size(btn_cancelar, 160, 80);
    lv_obj_align_to(btn_cancelar, btn_aceptar, LV_ALIGN_OUT_LEFT_MID, -20, 0);
    lv_obj_set_style_bg_color(btn_cancelar, lv_color_hex(0xE63946), 0); // Rojo retro
    lv_obj_set_style_radius(btn_cancelar, 0, 0);
    lv_obj_set_style_border_width(btn_cancelar, 4, 0);
    lv_obj_set_style_border_color(btn_cancelar, COLOR_BORDE_RETRO, 0);

    // CAMBIO AQUÍ: Le asignamos una nueva función para que borre al hacer clic
    lv_obj_add_event_cb(btn_cancelar, cb_borrar_alarma, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_cancelar = lv_label_create(btn_cancelar);
    // CAMBIO AQUÍ: Ahora el texto dice BORRAR
    lv_label_set_text(label_cancelar, "BORRAR");
    lv_obj_set_style_text_font(label_cancelar, &Minecraft16, 0);
    lv_obj_set_style_text_color(label_cancelar, lv_color_white(), 0);
    lv_obj_align(label_cancelar, LV_ALIGN_CENTER, 0, 0);

    clock_screen_update();
}

void clock_screen_update(void)
{
    if (label_hora == NULL)
        return;

    time_t ahora = time(NULL);
    struct tm *t = localtime(&ahora);

    char buf_hora[6];
    snprintf(buf_hora, sizeof(buf_hora), "%02d:%02d", t->tm_hour, t->tm_min);
    lv_label_set_text(label_hora, buf_hora);

    static const char *dias[] = {"Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado"};
    static const char *meses[] = {"Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio",
                                  "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"};

    char buf_fecha[50];
    snprintf(buf_fecha, sizeof(buf_fecha), "%s | %d de %s",
             dias[t->tm_wday],
             t->tm_mday,
             meses[t->tm_mon]);
    lv_label_set_text(label_fecha, buf_fecha);

    // ── backend de alarma confirmada ──
    if (alarma_activa)
    {
        if (t->tm_hour == alarma_confirmada_hora && t->tm_min == alarma_confirmada_min && t->tm_sec == 0)
        {
            printf("\n[ALERTA DESPERTADOR] ¡¡RING RING!! Despierta Hugo, son las %02d:%02d\n", alarma_confirmada_hora, alarma_confirmada_min);
        }
    }

    // ── Lógica de Progreso del MP3 de Nuria ──
    if (label_tiempo_progreso != NULL)
    {
        cancion_segundos_actual++;

        if (cancion_segundos_actual > cancion_segundos_total)
        {
            cancion_segundos_actual = 0;
        }

        // Calculamos los minutos y segundos por donde va la canción
        int min_actual = cancion_segundos_actual / 60;
        int seg_actual = cancion_segundos_actual % 60;

        // ── ESTO ES LO NUEVO: Calculamos los minutos y segundos TOTALES ──
        int min_total = cancion_segundos_total / 60;
        int seg_total = cancion_segundos_total % 60;

        // Lo pintamos dinámicamente ("Actual - Total")
        char buf_tiempo[30];
        snprintf(buf_tiempo, sizeof(buf_tiempo), "%d:%02d - %d:%02d", min_actual, seg_actual, min_total, seg_total);
        lv_label_set_text(label_tiempo_progreso, buf_tiempo);
    }
}