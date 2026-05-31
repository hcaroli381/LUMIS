#include "clock_screen.h"
#include <time.h>
#include <stdio.h>

// fuentes

LV_FONT_DECLARE(Minecraft48);
LV_FONT_DECLARE(Minecraft16);
LV_FONT_DECLARE(Minecraft24);
LV_IMG_DECLARE(lumisAssistant);

// ─── PALETA DE COLORES PIXEL ART (Estilo RPG Retro) ─────────────────
#define COLOR_FONDO lv_color_hex(0xF4ECCF)       // Crema suave de fondo (estilo consola portátil)
#define COLOR_BORDE_RETRO lv_color_hex(0x2C2C2C) // Casi negro para los contornos pixelados
#define COLOR_CAJA_HORA lv_color_hex(0xD2A14E)   // Oro viejo/marrón retro para el bloque de la hora
#define COLOR_TEXTO_DARK lv_color_hex(0x2C2C2C)  // Texto principal oscuro

static lv_obj_t *label_hora = NULL;
static lv_obj_t *label_fecha = NULL;
static lv_timer_t *timer_reloj = NULL;
static lv_obj_t *panel_musica;
static lv_obj_t *label_modo;
static int modo_actual = 0; // 0 = Silencio, 1 = Estudio, 2 = Lata34
static lv_obj_t *panel_despertador;
static lv_obj_t *label_alarma_tiempo;
static int alarma_hora = 7;
static int alarma_min = 30;

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
}

static void cb_cambiar_modo(lv_event_t *e)
{
    modo_actual = (modo_actual + 1) % 3;

    if (modo_actual == 0)
    {
        lv_label_set_text(label_modo, "Silencio");
    }
    else if (modo_actual == 1)
    {
        lv_label_set_text(label_modo, "Estudio");
    }
    else if (modo_actual == 2)
    {
        lv_label_set_text(label_modo, "Lata34");
    }
}
static void cb_toggle_despertador(lv_event_t *e)
{
    if (lv_obj_has_flag(panel_despertador, LV_OBJ_FLAG_HIDDEN))
    {
        lv_obj_clear_flag(panel_despertador, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(panel_despertador, LV_OBJ_FLAG_HIDDEN);
    }
}

static void _actualizar_texto_alarma(void)
{
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", alarma_hora, alarma_min);
    lv_label_set_text(label_alarma_tiempo, buf);
}

static void cb_modificar_alarma(lv_event_t *e)
{
    // Recuperamos qué botón ha sido pulsado gracias al user_data que le pasaremos
    long accion = (long)lv_event_get_user_data(e);

    if (accion == 1)
    { // + Hora
        alarma_hora = (alarma_hora + 1) % 24;
    }
    else if (accion == 2)
    { // - Hora
        alarma_hora = (alarma_hora - 1 + 24) % 24;
    }
    else if (accion == 3)
    {                                       // + Minuto
        alarma_min = (alarma_min + 5) % 60; // Avanza de 5 en 5 minutos para que sea más cómodo
    }
    else if (accion == 4)
    { // - Minuto
        alarma_min = (alarma_min - 5 + 60) % 60;
    }

    _actualizar_texto_alarma();
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
    lv_obj_set_pos(bloque_hora, 40, 40); // Un pelín más separado en la pantalla grande
    lv_obj_set_style_bg_color(bloque_hora, COLOR_CAJA_HORA, 0);
    lv_obj_set_style_bg_opa(bloque_hora, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bloque_hora, 0, 0);
    lv_obj_set_style_border_width(bloque_hora, 5, 0);
    lv_obj_set_style_border_color(bloque_hora, COLOR_BORDE_RETRO, 0);
    lv_obj_set_style_pad_all(bloque_hora, 0, 0);

    // Texto de la hora
    label_hora = lv_label_create(bloque_hora);
    lv_obj_set_style_text_color(label_hora, COLOR_TEXTO_DARK, 0);
    lv_obj_set_style_text_font(label_hora, &Minecraft48, 0);
    lv_label_set_text(label_hora, "00:00");
    lv_obj_align(label_hora, LV_ALIGN_CENTER, 0, 0);

    // Texto de la fecha (Justo debajo del reloj)
    // ── Etiqueta de la fecha (¡Más grande y estilizada!) ──
    // ── Etiqueta de la fecha (¡AHORA MÁS GRANDE!) ──
    label_fecha = lv_label_create(pantalla);
    lv_obj_set_style_text_color(label_fecha, COLOR_TEXTO_DARK, 0);

    // CAMBIO: Le asignamos la fuente GRANDE (Minecraft48) igual que al reloj
    lv_obj_set_style_text_font(label_fecha, &Minecraft48, 0);

    // Ajustamos la posición (X=40, Y=170) para que no se pise con la hora
    lv_obj_set_pos(label_fecha, 40, 170);

    // TRUCO RETRO: Podemos hacer que el texto se muestre en MAYÚSCULAS
    // modificando el formato en la función de actualización de abajo para que tenga más impacto visual.

    // ── 🤖 HUECO PARA EL ROBOT ASISTENTE (Lado derecho inferior) ──
    // Creamos una caja negra pixelada que albergará el dibujo del robot
    lv_obj_t *contenedor_robot = lv_obj_create(pantalla);
    lv_obj_set_size(contenedor_robot, 300, 300);
    lv_obj_set_pos(contenedor_robot, 650, 250);                  // Abajo a la derecha
    lv_obj_set_style_bg_color(contenedor_robot, COLOR_FONDO, 0); // Fondo blanco para que resalte
    lv_obj_set_style_radius(contenedor_robot, 0, 0);             // Cuadrado píxel
    lv_obj_set_style_border_width(contenedor_robot, 0, 0);
    lv_obj_set_style_border_color(contenedor_robot, COLOR_BORDE_RETRO, 0);

    // ── 🤖 IMAGEN REAL DEL ROBOT ASISTENTE ──
    lv_obj_t *img_robot = lv_img_create(pantalla);
    lv_img_set_src(img_robot, &lumisAssistant);
    lv_obj_set_pos(img_robot, 680, 230); // Posición ajustada a la derecha de la pantalla

    // ── 💬 BOCADILLO DE TEXTO DEL ROBOT (Ajustado para tipografía pixel) ──
    lv_obj_t *bocadillo = lv_obj_create(pantalla);
    lv_obj_set_size(bocadillo, 640, 130);
    lv_obj_set_pos(bocadillo, 40, 260);

    lv_obj_set_style_bg_color(bocadillo, lv_color_white(), 0);

    // El radio genera las esquinas curvas con escalones de píxeles puros
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
    // Timer de actualización activa
    timer_reloj = lv_timer_create(_actualizar_reloj, 1000, NULL);
    clock_screen_update();

    // Boton disparador flotante en la esquina inferior izquierda
    lv_obj_t *btn_nota = lv_button_create(pantalla);
    lv_obj_set_size(btn_nota, 60, 60);
    lv_obj_set_pos(btn_nota, 40, 500);
    lv_obj_set_style_bg_color(btn_nota, lv_color_hex(0xD4A373), 0);
    lv_obj_set_style_radius(btn_nota, 0, 0);
    lv_obj_set_style_border_width(btn_nota, 4, 0);
    lv_obj_set_style_border_color(btn_nota, COLOR_BORDE_RETRO, 0);
    lv_obj_add_event_cb(btn_nota, cb_toggle_reproductor, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_nota = lv_label_create(btn_nota);
    lv_label_set_text(label_nota, "M"); // Aqui ira el icono, de momento una M de Musica
    lv_obj_set_style_text_font(label_nota, &Minecraft24, 0);
    lv_obj_set_style_text_color(label_nota, COLOR_TEXTO_DARK, 0);
    lv_obj_align(label_nota, LV_ALIGN_CENTER, 0, 0);

    // Panel oculto que contiene los controles
    // Panel a pantalla completa para el reproductor (ocupa casi todo el frontal)
    panel_musica = lv_obj_create(pantalla);
    lv_obj_set_size(panel_musica, 944, 520);
    lv_obj_set_pos(panel_musica, 40, 40);
    lv_obj_set_style_bg_color(panel_musica, lv_color_white(), 0);
    lv_obj_set_style_radius(panel_musica, 24, 0); // Curvatura pixelada más marcada por el tamaño
    lv_obj_set_style_border_width(panel_musica, 6, 0);
    lv_obj_set_style_border_color(panel_musica, COLOR_BORDE_RETRO, 0);
    lv_obj_add_flag(panel_musica, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(panel_musica, LV_OBJ_FLAG_SCROLLABLE);

    // Título superior del menú
    lv_obj_t *label_titulo = lv_label_create(panel_musica);
    lv_label_set_text(label_titulo, "REPRODUCTOR MULTIMEDIA");
    lv_obj_set_style_text_font(label_titulo, &Minecraft24, 0);
    lv_obj_set_style_text_color(label_titulo, COLOR_TEXTO_DARK, 0);
    lv_obj_align(label_titulo, LV_ALIGN_TOP_MID, 0, 20);

    // 1. Cabecera pequeña de reproducción
    lv_obj_t *label_now_playing = lv_label_create(panel_musica);
    lv_label_set_text(label_now_playing, "NOW PLAYING:");
    lv_obj_set_style_text_font(label_now_playing, &Minecraft16, 0);
    lv_obj_set_style_text_color(label_now_playing, lv_color_hex(0x8B8C89), 0);
    lv_obj_align(label_now_playing, LV_ALIGN_CENTER, 0, -110);

    // 2. Título de la canción (Subido a la fuente de 48px para que sea el rey de la pantalla)
    lv_obj_t *label_cancion = lv_label_create(panel_musica);
    lv_label_set_text(label_cancion, "Intro (Lata34 Mix)");
    lv_obj_set_style_text_font(label_cancion, &Minecraft48, 0);
    lv_obj_set_style_text_color(label_cancion, COLOR_TEXTO_DARK, 0);
    lv_obj_align(label_cancion, LV_ALIGN_CENTER, 0, -60);

    // 3. Autores o Artistas (Bajado un pelín a posición -10)
    lv_obj_t *label_autor = lv_label_create(panel_musica);
    lv_label_set_text(label_autor, "Hugo & Friends");
    lv_obj_set_style_text_font(label_autor, &Minecraft24, 0);
    lv_obj_set_style_text_color(label_autor, lv_color_hex(0x666666), 0);
    lv_obj_align(label_autor, LV_ALIGN_CENTER, 0, -10);

    // 4. Modo activo actual (Bajado a posición 40 para que no pise nada)
    label_modo = lv_label_create(panel_musica);
    lv_label_set_text(label_modo, "Silencio");
    lv_obj_set_style_text_font(label_modo, &Minecraft24, 0);
    lv_obj_set_style_text_color(label_modo, lv_color_hex(0xD2A14E), 0); // Color marrón ocre a juego con el reloj
    lv_obj_align(label_modo, LV_ALIGN_CENTER, 0, 40);

    // Botón enorme para CAMBIAR MODO
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
    lv_obj_set_style_text_color(label_btn_modo, COLOR_TEXTO_DARK, 0);
    lv_obj_align(label_btn_modo, LV_ALIGN_CENTER, 0, 0);

    // Botón enorme de PLAY / PAUSE
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
    lv_obj_set_style_text_color(label_play, COLOR_TEXTO_DARK, 0);
    lv_obj_align(label_play, LV_ALIGN_CENTER, 0, 0);

    // Botón enorme de SIGUIENTE CANCIÓN
    lv_obj_t *btn_next = lv_button_create(panel_musica);
    lv_obj_set_size(btn_next, 160, 80);
    lv_obj_align_to(btn_next, btn_play, LV_ALIGN_OUT_RIGHT_MID, 40, 0);
    lv_obj_set_style_bg_color(btn_next, lv_color_hex(0xCCD5AE), 0);
    lv_obj_set_style_radius(btn_next, 0, 0);
    lv_obj_set_style_border_width(btn_next, 4, 0);
    lv_obj_set_style_border_color(btn_next, COLOR_BORDE_RETRO, 0);

    lv_obj_t *label_next = lv_label_create(btn_next);
    lv_label_set_text(label_next, ">>");
    lv_obj_set_style_text_font(label_next, &Minecraft24, 0);
    lv_obj_set_style_text_color(label_next, COLOR_TEXTO_DARK, 0);
    lv_obj_align(label_next, LV_ALIGN_CENTER, 0, 0);

    // Botón para VOLVER / CERRAR el menú
    lv_obj_t *btn_cerrar = lv_button_create(panel_musica);
    lv_obj_set_size(btn_cerrar, 140, 80);
    lv_obj_align(btn_cerrar, LV_ALIGN_BOTTOM_RIGHT, -60, -40);
    lv_obj_set_style_bg_color(btn_cerrar, lv_color_hex(0xE63946), 0); // Color rojizo para salir
    lv_obj_set_style_radius(btn_cerrar, 0, 0);
    lv_obj_set_style_border_width(btn_cerrar, 4, 0);
    lv_obj_set_style_border_color(btn_cerrar, COLOR_BORDE_RETRO, 0);
    lv_obj_add_event_cb(btn_cerrar, cb_toggle_reproductor, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_cerrar = lv_label_create(btn_cerrar);
    lv_label_set_text(label_cerrar, "VOLVER");
    lv_obj_set_style_text_font(label_cerrar, &Minecraft16, 0);
    lv_obj_set_style_text_color(label_cerrar, lv_color_white(), 0);
    lv_obj_align(label_cerrar, LV_ALIGN_CENTER, 0, 0);

    // Botón disparador del Despertador (Esquina inferior derecha, simétrico al de música)
    // Botón disparador del Despertador (Movido al lado del de música para no pisar al robot)
    lv_obj_t *btn_campana = lv_button_create(pantalla);
    lv_obj_set_size(btn_campana, 60, 60);
    lv_obj_set_pos(btn_campana, 120, 500); // Reubicado a X=120 (Al lado de la M de música)
    lv_obj_set_style_bg_color(btn_campana, lv_color_hex(0xD4A373), 0);
    lv_obj_set_style_radius(btn_campana, 0, 0);
    lv_obj_set_style_border_width(btn_campana, 4, 0);
    lv_obj_set_style_border_color(btn_campana, COLOR_BORDE_RETRO, 0);
    lv_obj_add_event_cb(btn_campana, cb_toggle_despertador, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_campana = lv_label_create(btn_campana);
    lv_label_set_text(label_campana, "A");
    lv_obj_set_style_text_font(label_campana, &Minecraft24, 0);
    lv_obj_set_style_text_color(label_campana, COLOR_TEXTO_DARK, 0);
    lv_obj_align(label_campana, LV_ALIGN_CENTER, 0, 0);

    // Panel a pantalla completa para el Despertador
    // Panel a pantalla completa para el Despertador
    panel_despertador = lv_obj_create(pantalla);
    lv_obj_set_size(panel_despertador, 944, 520);
    lv_obj_set_pos(panel_despertador, 40, 40);
    lv_obj_set_style_bg_color(panel_despertador, lv_color_white(), 0);
    lv_obj_set_style_radius(panel_despertador, 24, 0);

    // Dejamos solo esta línea que es la buena:
    lv_obj_set_style_border_width(panel_despertador, 6, 0);

    lv_obj_set_style_border_color(panel_despertador, COLOR_BORDE_RETRO, 0);
    lv_obj_add_flag(panel_despertador, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(panel_despertador, LV_OBJ_FLAG_SCROLLABLE);

    // Título superior
    lv_obj_t *label_titulo_desp = lv_label_create(panel_despertador);
    lv_label_set_text(label_titulo_desp, "CONFIGURACION DE ALARMA");
    lv_obj_set_style_text_font(label_titulo_desp, &Minecraft24, 0);
    lv_obj_set_style_text_color(label_titulo_desp, COLOR_TEXTO_DARK, 0);
    lv_obj_align(label_titulo_desp, LV_ALIGN_TOP_MID, 0, 20);

    // Indicador de la hora de la alarma gigante en el centro
    label_alarma_tiempo = lv_label_create(panel_despertador);
    lv_label_set_text(label_alarma_tiempo, "07:30");
    lv_obj_set_style_text_font(label_alarma_tiempo, &Minecraft48, 0);
    lv_obj_set_style_text_color(label_alarma_tiempo, COLOR_TEXTO_DARK, 0);
    lv_obj_align(label_alarma_tiempo, LV_ALIGN_CENTER, 0, -40);

    // Botón MAS HORAS (+) -> Pasa el ID 1 al callback
    lv_obj_t *btn_hora_up = lv_button_create(panel_despertador);
    lv_obj_set_size(btn_hora_up, 80, 50);
    lv_obj_align(btn_hora_up, LV_ALIGN_CENTER, -60, -110);
    lv_obj_set_style_bg_color(btn_hora_up, lv_color_hex(0xE9EDC9), 0);
    lv_obj_set_style_radius(btn_hora_up, 0, 0);
    lv_obj_set_style_border_width(btn_hora_up, 3, 0);
    lv_obj_set_style_border_color(btn_hora_up, COLOR_BORDE_RETRO, 0);
    lv_obj_add_event_cb(btn_hora_up, cb_modificar_alarma, LV_EVENT_CLICKED, (void *)1);

    lv_obj_t *label_h_up = lv_label_create(btn_hora_up);
    lv_label_set_text(label_h_up, "+");
    lv_obj_set_style_text_font(label_h_up, &Minecraft24, 0);
    lv_obj_set_style_text_color(label_h_up, COLOR_TEXTO_DARK, 0);
    lv_obj_align(label_h_up, LV_ALIGN_CENTER, 0, 0);

    // Botón MENOS HORAS (-) -> Pasa el ID 2 al callback
    lv_obj_t *btn_hora_down = lv_button_create(panel_despertador);
    lv_obj_set_size(btn_hora_down, 80, 50);
    lv_obj_align(btn_hora_down, LV_ALIGN_CENTER, -60, 30);
    lv_obj_set_style_bg_color(btn_hora_down, lv_color_hex(0xE9EDC9), 0);
    lv_obj_set_style_radius(btn_hora_down, 0, 0);
    lv_obj_set_style_border_width(btn_hora_down, 3, 0);
    lv_obj_set_style_border_color(btn_hora_down, COLOR_BORDE_RETRO, 0);
    lv_obj_add_event_cb(btn_hora_down, cb_modificar_alarma, LV_EVENT_CLICKED, (void *)2);

    lv_obj_t *label_h_down = lv_label_create(btn_hora_down);
    lv_label_set_text(label_h_down, "-");
    lv_obj_set_style_text_font(label_h_down, &Minecraft24, 0);
    lv_obj_set_style_text_color(label_h_down, COLOR_TEXTO_DARK, 0);
    lv_obj_align(label_h_down, LV_ALIGN_CENTER, 0, 0);

    // Botón MAS MINUTOS (+) -> Pasa el ID 3 al callback
    lv_obj_t *btn_min_up = lv_button_create(panel_despertador);
    lv_obj_set_size(btn_min_up, 80, 50);
    lv_obj_align(btn_min_up, LV_ALIGN_CENTER, 60, -110);
    lv_obj_set_style_bg_color(btn_min_up, lv_color_hex(0xE9EDC9), 0);
    lv_obj_set_style_radius(btn_min_up, 0, 0);
    lv_obj_set_style_border_width(btn_min_up, 3, 0);
    lv_obj_set_style_border_color(btn_min_up, COLOR_BORDE_RETRO, 0);
    lv_obj_add_event_cb(btn_min_up, cb_modificar_alarma, LV_EVENT_CLICKED, (void *)3);

    lv_obj_t *label_m_up = lv_label_create(btn_min_up);
    lv_label_set_text(label_m_up, "+");
    lv_obj_set_style_text_font(label_m_up, &Minecraft24, 0);
    lv_obj_set_style_text_color(label_m_up, COLOR_TEXTO_DARK, 0);
    lv_obj_align(label_m_up, LV_ALIGN_CENTER, 0, 0);

    // Botón MENOS MINUTOS (-) -> Pasa el ID 4 al callback
    lv_obj_t *btn_min_down = lv_button_create(panel_despertador);
    lv_obj_set_size(btn_min_down, 80, 50);
    lv_obj_align(btn_min_down, LV_ALIGN_CENTER, 60, 30);
    lv_obj_set_style_bg_color(btn_min_down, lv_color_hex(0xE9EDC9), 0);
    lv_obj_set_style_radius(btn_min_down, 0, 0);
    lv_obj_set_style_border_width(btn_min_down, 3, 0);
    lv_obj_set_style_border_color(btn_min_down, COLOR_BORDE_RETRO, 0);
    lv_obj_add_event_cb(btn_min_down, cb_modificar_alarma, LV_EVENT_CLICKED, (void *)4);

    lv_obj_t *label_m_down = lv_label_create(btn_min_down);
    lv_label_set_text(label_m_down, "-");
    lv_obj_set_style_text_font(label_m_down, &Minecraft24, 0);
    lv_obj_set_style_text_color(label_m_down, COLOR_TEXTO_DARK, 0);
    lv_obj_align(label_m_down, LV_ALIGN_CENTER, 0, 0);

    // Botón para VOLVER / CERRAR el menú del despertador
    lv_obj_t *btn_cerrar_desp = lv_button_create(panel_despertador);
    lv_obj_set_size(btn_cerrar_desp, 140, 80);
    lv_obj_align(btn_cerrar_desp, LV_ALIGN_BOTTOM_RIGHT, -60, -40);
    lv_obj_set_style_bg_color(btn_cerrar_desp, lv_color_hex(0xE63946), 0);
    lv_obj_set_style_radius(btn_cerrar_desp, 0, 0);
    lv_obj_set_style_border_width(btn_cerrar_desp, 4, 0);
    lv_obj_set_style_border_color(btn_cerrar_desp, COLOR_BORDE_RETRO, 0);
    lv_obj_add_event_cb(btn_cerrar_desp, cb_toggle_despertador, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label_cerrar_desp = lv_label_create(btn_cerrar_desp);
    lv_label_set_text(label_cerrar_desp, "VOLVER");
    lv_obj_set_style_text_font(label_cerrar_desp, &Minecraft16, 0);
    lv_obj_set_style_text_color(label_cerrar_desp, lv_color_white(), 0);
    lv_obj_align(label_cerrar_desp, LV_ALIGN_CENTER, 0, 0);
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

    // Reemplaza la parte final de clock_screen_update por esta:
    char buf_fecha[50];
    // Le añadimos un separador o formato limpio estilo RPG
    snprintf(buf_fecha, sizeof(buf_fecha), "%s | %d de %s",
             dias[t->tm_wday],
             t->tm_mday,
             meses[t->tm_mon]);
    lv_label_set_text(label_fecha, buf_fecha);
}