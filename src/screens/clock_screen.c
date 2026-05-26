#include "clock_screen.h"
#include <time.h>
#include <stdio.h>

// fuentes

LV_FONT_DECLARE(Minecraft48);
LV_FONT_DECLARE(Minecraft16);

// ─── PALETA DE COLORES PIXEL ART (Estilo RPG Retro) ─────────────────
#define COLOR_FONDO         lv_color_hex(0xF4ECCF) // Crema suave de fondo (estilo consola portátil)
#define COLOR_BORDE_RETRO   lv_color_hex(0x2C2C2C) // Casi negro para los contornos pixelados
#define COLOR_CAJA_HORA     lv_color_hex(0xD2A14E) // Oro viejo/marrón retro para el bloque de la hora
#define COLOR_TEXTO_DARK    lv_color_hex(0x2C2C2C) // Texto principal oscuro

static lv_obj_t * label_hora  = NULL;
static lv_obj_t * label_fecha = NULL;
static lv_timer_t * timer_reloj = NULL;

static void _actualizar_reloj(lv_timer_t * timer) {
    (void)timer;
    clock_screen_update();
}

void clock_screen_create(void) {
    lv_obj_t * pantalla = lv_scr_act();
    // Aplicamos el color crema de fondo
    lv_obj_set_style_bg_color(pantalla, COLOR_FONDO, 0);
    lv_obj_set_style_bg_opa(pantalla, LV_OPA_COVER, 0);

    // ── CAJA DE LA HORA ESTILO PIXEL ART ──
    lv_obj_t * bloque_hora = lv_obj_create(pantalla);
    lv_obj_set_size(bloque_hora, 280, 110);
    lv_obj_set_pos(bloque_hora, 20, 20); // Separado del borde como en los juegos
    
    // Configuración estética: Fondo sólido + Borde grueso sin esquinas redondeadas
    lv_obj_set_style_bg_color(bloque_hora, COLOR_CAJA_HORA, 0);
    lv_obj_set_style_bg_opa(bloque_hora, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bloque_hora, 0, 0); // Totalmente cuadrado (Píxel Puro)
    
    // Contorno negro grueso característico del Pixel Art
    lv_obj_set_style_border_width(bloque_hora, 5, 0); 
    lv_obj_set_style_border_color(bloque_hora, COLOR_BORDE_RETRO, 0);
    lv_obj_set_style_pad_all(bloque_hora, 0, 0);

    // ── TEXTO DE LA HORA ──
    label_hora = lv_label_create(bloque_hora);
    lv_obj_set_style_text_color(label_hora, COLOR_TEXTO_DARK, 0);
    // Nota: De momento dejamos la fuente por defecto. En el siguiente paso la cambiaremos por una pixelada real.
    lv_obj_set_style_text_font(label_hora, &Minecraft48, 0); 
    lv_label_set_text(label_hora, "00:00");
    lv_obj_align(label_hora, LV_ALIGN_CENTER, 0, 0);

    // ── TEXTO DE LA FECHA (Justo debajo) ──
    label_fecha = lv_label_create(pantalla);
    lv_obj_set_style_text_color(label_fecha, COLOR_TEXTO_DARK, 0);
    lv_obj_set_style_text_font(label_fecha, &Minecraft16, 0);
    lv_label_set_text(label_fecha, "Lunes, 1 Enero");
    // Lo posicionamos alineado con la caja (X=25) y un poco más abajo (Y=145)
    lv_obj_set_pos(label_fecha, 25, 145);

    // Timer de actualización activa
    timer_reloj = lv_timer_create(_actualizar_reloj, 1000, NULL);
    clock_screen_update();
}

void clock_screen_update(void) {
    if (label_hora == NULL) return;

    time_t ahora = time(NULL);
    struct tm * t = localtime(&ahora);

    char buf_hora[6];
    snprintf(buf_hora, sizeof(buf_hora), "%02d:%02d", t->tm_hour, t->tm_min);
    lv_label_set_text(label_hora, buf_hora);

    static const char * dias[]  = {"Domingo","Lunes","Martes","Miércoles","Jueves","Viernes","Sábado"};
    static const char * meses[] = {"Enero","Febrero","Marzo","Abril","Mayo","Junio",
                                    "Julio","Agosto","Septiembre","Octubre","Noviembre","Diciembre"};

    char buf_fecha[40];
    snprintf(buf_fecha, sizeof(buf_fecha), "%s, %d %s",
             dias[t->tm_wday],
             t->tm_mday,
             meses[t->tm_mon]);
    lv_label_set_text(label_fecha, buf_fecha);
}