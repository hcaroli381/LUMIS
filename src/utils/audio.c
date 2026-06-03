#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Implementaciones de utilidades de audio.
// Comentarios esenciales en español explican la lógica principal.

static int estado_pausado = 0;

int obtener_duracion_mp3(const char *ruta_archivo)
{
    FILE *f = fopen(ruta_archivo, "rb");
    if (!f)
        return 180;

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
            int modo = (frame[3] >> 6) & 0x03;

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

    int side_info_size = (canales == 1) ? 17 : 32;
    long xing_offset = pos + 4 + side_info_size;

    fseek(f, xing_offset, SEEK_SET);
    unsigned char xing_buf[120];
    int xing_leidos = (int)fread(xing_buf, 1, sizeof(xing_buf), f);

    int xing_pos = -1;
    for (int i = 0; i <= xing_leidos - 4; i++)
    {
        if ((memcmp(xing_buf + i, "Xing", 4) == 0) || (memcmp(xing_buf + i, "Info", 4) == 0))
        {
            xing_pos = i;
            break;
        }
    }

    if (xing_pos >= 0 && xing_pos + 8 <= xing_leidos)
    {
        unsigned char *x = xing_buf + xing_pos;
        unsigned long flags = ((unsigned long)x[4] << 24) | ((unsigned long)x[5] << 16) |
                              ((unsigned long)x[6] << 8) | (unsigned long)x[7];

        if (flags & 0x01)
        {
            unsigned long num_frames = ((unsigned long)x[8] << 24) |
                                       ((unsigned long)x[9] << 16) |
                                       ((unsigned long)x[10] << 8) |
                                       (unsigned long)x[11];

            int duracion = (int)((double)num_frames * 1152.0 / samplerate);
            fclose(f);
            return duracion > 0 ? duracion : 180;
        }
    }

    int idx_br = (frame[2] >> 4) & 0x0F;
    int bitrate_kbps = tabla_bitrates[idx_br];

    fseek(f, 0, SEEK_END);
    long tamano_audio = ftell(f) - offset_audio;
    fclose(f);

    if (bitrate_kbps >= 32 && bitrate_kbps <= 320)
    {
        int duracion = (int)((tamano_audio * 8L) / ((long)bitrate_kbps * 1000L));
        return duracion > 0 ? duracion : 180;
    }

    return (int)(tamano_audio / 24000);
}

int extraer_caratula_mp3(const char *ruta_mp3, const char *ruta_salida_jpg)
{
    FILE *f = fopen(ruta_mp3, "rb");
    if (!f)
        return 0;

    unsigned char header[10];
    if (fread(header, 1, 10, f) < 10)
    {
        fclose(f);
        return 0;
    }

    if (header[0] != 'I' || header[1] != 'D' || header[2] != '3')
    {
        fclose(f);
        return 0;
    }

    int version_mayor = header[3];
    long tag_size = ((header[6] & 0x7F) << 21) |
                    ((header[7] & 0x7F) << 14) |
                    ((header[8] & 0x7F) << 7) |
                    (header[9] & 0x7F);

    unsigned char *tag_data = (unsigned char *)malloc(tag_size);
    if (!tag_data)
    {
        fclose(f);
        return 0;
    }

    long leidos = (long)fread(tag_data, 1, tag_size, f);
    fclose(f);
    if (leidos <= 0)
    {
        free(tag_data);
        return 0;
    }

    long pos = 0;
    int encontrado = 0;
    while (pos < tag_size - 10)
    {
        char frame_id[5];
        memcpy(frame_id, tag_data + pos, 4);
        frame_id[4] = '\0';

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

        if (frame_size <= 0 || pos + 10 + frame_size > tag_size)
            break;

        if (strcmp(frame_id, "APIC") == 0)
        {
            unsigned char *apic = tag_data + pos + 10;
            long apic_size = frame_size;

            unsigned char encoding = apic[0];
            long offset = 1;

            // 1. Saltar el MIME Type (ej: "image/jpeg")
            char mime[64] = "";
            int mi = 0;
            while (offset < apic_size && apic[offset] != '\0' && mi < 63)
                mime[mi++] = apic[offset++];
            mime[mi] = '\0';
            offset++; // Saltar el byte nulo terminador del MIME

            // 2. Saltar el Picture Type (1 byte)
            if (offset < apic_size)
            {
                offset++;
            }

            // 3. Saltar la Descripción según el tipo de codificación de texto
            if (encoding == 1 || encoding == 2) // UTF-16
            {
                while (offset < apic_size - 1 && !(apic[offset] == 0x00 && apic[offset + 1] == 0x00))
                    offset += 2;
                offset += 2; // Saltar el doble nulo
            }
            else // UTF-8 o ISO-8859-1
            {
                while (offset < apic_size && apic[offset] != '\0')
                    offset++;
                offset++; // Saltar el byte nulo
            }

            // 4. Buscar el inicio real de la imagen JPEG (Marcador FF D8)
            long jpg_start = -1;
            for (long i = offset; i < apic_size - 1; i++)
            {
                if (apic[i] == 0xFF && apic[i + 1] == 0xD8)
                {
                    jpg_start = i;
                    break;
                }
            }

            // 5. Encontrar el final real (FF D9) buscando hacia atrás para dejar fuera el padding
            if (jpg_start >= 0)
            {
                long jpg_end = apic_size;

                // Recorremos hacia atrás desde el final del frame, pero nos detenemos en el inicio del JPG
                for (long i = apic_size - 2; i >= jpg_start; i--)
                {
                    if (apic[i] == 0xFF && apic[i + 1] == 0xD9)
                    {
                        jpg_end = i + 2; // El archivo termina justo tras el marcador D9
                        break;
                    }
                }

                // 6. Volcar al disco únicamente los bytes de la imagen limpia
                FILE *out = fopen(ruta_salida_jpg, "wb");
                if (out)
                {
                    fwrite(apic + jpg_start, 1, jpg_end - jpg_start, out);
                    fclose(out);
                    encontrado = 1;
                }
            }
            break; // Salimos del bucle al procesar la carátula
        }

        pos += 10 + frame_size;
    }

    free(tag_data);
    return encontrado;
}
// ─── IMPLEMENTACIÓN DE REPRODUCCIÓN (AISLADO PARA ARDUINO) ───

// Variable interna para guardar el estado del reproductor

void audio_play(const char *ruta_mp3)
{
    audio_stop();
    if (ruta_mp3 == NULL)
        return;

    estado_pausado = 0; // Al empezar una canción nueva, NO está pausada

#if defined(__linux__)
    char comando[512];
    const char *ruta_real = ruta_mp3;

    if (strncmp(ruta_real, "A:", 2) == 0)
    {
        ruta_real += 2;
    }

    snprintf(comando, sizeof(comando), "mpv --no-video \"%s\" > /dev/null 2>&1 &", ruta_real);
    int res = system(comando);
    (void)res;
#else
    // --- Arduino real ---
#endif
}

void audio_stop(void)
{
    estado_pausado = 0;
#if defined(__linux__)
    int res = system("killall mpv > /dev/null 2>&1");
    (void)res;
#else
    // --- Arduino real ---
#endif
}

void audio_pause_toggle(void)
{
#if defined(__linux__)
    int res;
    if (estado_pausado == 0)
    {
        // Si estaba sonando, mandamos señal de congelar (SIGSTOP)
        res = system("killall -SIGSTOP mpv > /dev/null 2>&1");
        estado_pausado = 1;
    }
    else
    {
        // Si estaba pausado, mandamos señal de continuar (SIGCONT)
        res = system("killall -SIGCONT mpv > /dev/null 2>&1");
        estado_pausado = 0;
    }
    (void)res;
#else
    // --- Arduino real ---
    // estado_pausado = !estado_pausado;
#endif
}

int audio_is_paused(void)
{
    return estado_pausado;
}
