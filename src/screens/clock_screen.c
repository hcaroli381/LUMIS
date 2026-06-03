#include "clock_screen.h"
#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include "../utils/alarma.h"
#include "../utils/audio.h"

// fuentes
LV_FONT_DECLARE(Minecraft48);
LV_FONT_DECLARE(Minecraft16);
LV_FONT_DECLARE(Minecraft24);
LV_IMG_DECLARE(lumisAssistant);

// Paleta de colores (pixel art)
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
    if (lv_obj_has_flag(panel_musica, LV_OBJ_FLAG_HIDDEN))
    {
        audio_stop();
    }
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

    // Reiniciar índice y recargar lista al cambiar modo
    indice_cancion_actual = 0;
    cargar_cancion_de_carpeta();
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

// Evento: confirmar alarma
static void cb_confirmar_alarma(lv_event_t *e)
{
    // Aplicamos la alarma mediante el módulo de alarma
    alarma_set(edit_hora, edit_min);

    // Actualizamos el texto de la interfaz
    char buf[20];
    snprintf(buf, sizeof(buf), "ALARM: %02d:%02d", edit_hora, edit_min);
    lv_label_set_text(label_info_alarma_principal, buf);

    // Ocultamos el panel de edición
    lv_obj_add_flag(panel_despertador, LV_OBJ_FLAG_HIDDEN);
    printf("[SISTEMA] Alarma fijada con éxito a las %02d:%02d\n", edit_hora, edit_min);
}
static void cb_borrar_alarma(lv_event_t *e)
{
    alarma_clear();
    lv_label_set_text(label_info_alarma_principal, "ALARM: OFF");
    lv_obj_add_flag(panel_despertador, LV_OBJ_FLAG_HIDDEN);
    printf("[SISTEMA] Alarma borrada por el usuario.\n");
}

#include <string.h>
#include "../utils/audio.h"

// Se han extraído las funciones de análisis MP3 a src/utils/audio.c
// Comentarios esenciales: `obtener_duracion_mp3` y `extraer_caratula_mp3` disponibles.

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
        // Control de error al abrir carpeta SD
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

        // Calcula la duración real del MP3 seleccionado
        char ruta_completa[256];
        snprintf(ruta_completa, sizeof(ruta_completa), "%s/%s", ruta_carpeta, nombre_encontrado);
        cancion_segundos_total = obtener_duracion_mp3(ruta_completa);
    }
    closedir(dir);

    // 3. ¡Actualizamos la pantalla con el archivo real!
    // Preparar extracción de carátula si existe
    if (nombre_encontrado[0] == '\0')
    {
        lv_obj_clear_flag(icono_mp3_global, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(img_caratula, LV_OBJ_FLAG_HIDDEN);
        return; // ← carpeta vacía, no intentamos extraer nada
    }

    // Extraer carátula APIC si está incrustada
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

        // Truco nativo de LVGL para vaciar la caché de imágenes sin funciones raras:
        lv_img_set_src(img_caratula, NULL); // Esto fuerza a LVGL a liberar la imagen anterior y cargar la nueva al siguiente set_src

        // Le pasamos el JPG puro recién extraído al simulador
        lv_img_set_src(img_caratula, ruta_lvgl_jpg);

        lv_image_header_t img_info;
        if (lv_image_decoder_get_info(ruta_lvgl_jpg, &img_info) == LV_RESULT_OK)
        {
            uint32_t lado = img_info.w > img_info.h ? img_info.w : img_info.h;
            if (lado > 172 && lado > 0)
            {
                uint16_t zoom = (uint16_t)((172 * 256) / lado);
                lv_img_set_zoom(img_caratula, zoom);
            }
            else
            {
                lv_img_set_zoom(img_caratula, 256);
            }
        }
    }
    else
    {
        // El MP3 no tiene carátula incrustada, mostramos la nota musical retro
        lv_obj_clear_flag(icono_mp3_global, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(img_caratula, LV_OBJ_FLAG_HIDDEN);
    }
    // Cada vez que cambia de canción, el segundero de Nuria vuelve a empezar en 0:00
    cancion_segundos_actual = 0;

    audio_play(ruta_completa);
}
static void cb_btn_play_click(lv_event_t *e)
{
    (void)e;
    audio_pause_toggle();
}

void clock_screen_create(void)
{
    alarma_init();
    lv_obj_t *pantalla = lv_scr_act();
    lv_obj_set_style_bg_color(pantalla, COLOR_FONDO, 0);
    lv_obj_set_style_bg_opa(pantalla, LV_OPA_COVER, 0);
    lv_obj_clear_flag(pantalla, LV_OBJ_FLAG_SCROLLABLE);

    // Caja de la hora (estilo pixel art)
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

    // Texto pequeño de estado de la alarma (lado del reloj)
    label_info_alarma_principal = lv_label_create(pantalla);
    lv_obj_set_pos(label_info_alarma_principal, 340, 80);                                // Situado justo a la derecha de la caja dorada
    lv_obj_set_style_text_color(label_info_alarma_principal, lv_color_hex(0xA3423C), 0); // Un rojo oscuro retro elegante
    lv_obj_set_style_text_font(label_info_alarma_principal, &Minecraft24, 0);            // Texto mediano legible
    lv_label_set_text(label_info_alarma_principal, "ALARM: OFF");                        // Por defecto apagada

    label_fecha = lv_label_create(pantalla);
    lv_obj_set_style_text_color(label_fecha, COLOR_TEXTO_DARK, 0);
    lv_obj_set_style_text_font(label_fecha, &Minecraft48, 0);
    lv_obj_set_pos(label_fecha, 40, 170);

    // Robot asistente (imagen)
    lv_obj_t *contenedor_robot = lv_obj_create(pantalla);
    lv_obj_set_size(contenedor_robot, 300, 300);
    lv_obj_set_pos(contenedor_robot, 650, 250);
    lv_obj_set_style_bg_color(contenedor_robot, COLOR_FONDO, 0);
    lv_obj_set_style_border_width(contenedor_robot, 0, 0);

    lv_obj_t *img_robot = lv_img_create(pantalla);
    lv_img_set_src(img_robot, &lumisAssistant);
    lv_obj_set_pos(img_robot, 680, 230);

    // Bocadillo de texto
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

    // Panel reproductor multimedia
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
    lv_obj_add_event_cb(btn_play, cb_btn_play_click, LV_EVENT_CLICKED, NULL);

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

    // Panel de configuración de la alarma
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

    // Botón borrar alarma: llama a cb_borrar_alarma
    lv_obj_add_event_cb(btn_cancelar, cb_borrar_alarma, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_cancelar = lv_label_create(btn_cancelar);
    // Etiqueta del botón borrar
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

    // Backend: comprobación de la alarma
    if (alarma_debe_sonar(t))
    {
        int ah, am;
        alarma_get(&ah, &am);
        printf("\n[ALERTA DESPERTADOR] ¡¡RING RING!! Despierta Hugo, son las %02d:%02d\n", ah, am);
    }

    // Lógica de progreso del MP3
    // Lógica de progreso del MP3 (Corregida para Arduino)
    if (label_tiempo_progreso != NULL && !lv_obj_has_flag(panel_musica, LV_OBJ_FLAG_HIDDEN))
    {
        // ¡AQUÍ ESTÁ EL TRUCO! Si la función nos dice que está pausado, saltamos el incremento
        if (audio_is_paused() == 0)
        {
            cancion_segundos_actual++;
        }

        if (cancion_segundos_actual > cancion_segundos_total)
        {
            cancion_segundos_actual = 0;
        }

        // Calculamos los minutos y segundos por donde va la canción
        int min_actual = cancion_segundos_actual / 60;
        int seg_actual = cancion_segundos_actual % 60;

        // Calcular minutos/segundos totales
        int min_total = cancion_segundos_total / 60;
        int seg_total = cancion_segundos_total % 60;

        // Lo pintamos dinámicamente ("Actual - Total")
        char buf_tiempo[30];
        snprintf(buf_tiempo, sizeof(buf_tiempo), "%d:%02d - %d:%02d", min_actual, seg_actual, min_total, seg_total);
        lv_label_set_text(label_tiempo_progreso, buf_tiempo);
    }
}