#include "clock_screen.h"
#include <time.h>
#include <stdio.h>

// fuentes

LV_FONT_DECLARE(Minecraft48);
LV_FONT_DECLARE(Minecraft16);
LV_FONT_DECLARE(Minecraft24);
LV_IMG_DECLARE(lumisAssistant);

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
    lv_obj_set_style_bg_color(pantalla, COLOR_FONDO, 0);
    lv_obj_set_style_bg_opa(pantalla, LV_OPA_COVER, 0);

    lv_obj_clear_flag(pantalla, LV_OBJ_FLAG_SCROLLABLE);
    // ── CAJA DE LA HORA ESTILO PIXEL ART ──
    lv_obj_t * bloque_hora = lv_obj_create(pantalla);
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
    lv_obj_t * contenedor_robot = lv_obj_create(pantalla);
    lv_obj_set_size(contenedor_robot, 300, 300);
    lv_obj_set_pos(contenedor_robot, 650, 250); // Abajo a la derecha
    lv_obj_set_style_bg_color(contenedor_robot, COLOR_FONDO, 0); // Fondo blanco para que resalte
    lv_obj_set_style_radius(contenedor_robot, 0, 0); // Cuadrado píxel
    lv_obj_set_style_border_width(contenedor_robot, 0, 0);
    lv_obj_set_style_border_color(contenedor_robot, COLOR_BORDE_RETRO, 0);
    
    // ── 🤖 IMAGEN REAL DEL ROBOT ASISTENTE ──
lv_obj_t * img_robot = lv_img_create(pantalla);
lv_img_set_src(img_robot, &lumisAssistant);
lv_obj_set_pos(img_robot, 680, 230); // Posición ajustada a la derecha de la pantalla

   // ── 💬 BOCADILLO DE TEXTO DEL ROBOT (Ajustado para tipografía pixel) ──
    lv_obj_t * bocadillo = lv_obj_create(pantalla);
    // Lo hacemos más bajito y compacto para que la letra de 16px rellene el espacio
    lv_obj_set_size(bocadillo, 580, 85);  
    lv_obj_set_pos(bocadillo, 40, 260);    

    lv_obj_set_style_bg_color(bocadillo, lv_color_white(), 0);
    lv_obj_set_style_radius(bocadillo, 0, 0); // Estilo píxel puro
    lv_obj_set_style_border_width(bocadillo, 4, 0);
    lv_obj_set_style_border_color(bocadillo, COLOR_BORDE_RETRO, 0);
    
    // IMPORTANTE: Quitamos márgenes internos del cuadro para ganar espacio real
    lv_obj_set_style_pad_all(bocadillo, 10, 0);

    // Texto de la frase
    lv_obj_t * label_frase = lv_label_create(bocadillo);
    lv_obj_set_style_text_font(label_frase, &Minecraft24, 0);
    lv_obj_set_style_text_color(label_frase, COLOR_TEXTO_DARK, 0);
    
    // Quitamos los acentos para evitar los molestos cuadraditos rotos
    lv_label_set_text(label_frase, "¡Hola Hugo! Esto es una frase de prueba para\nver como se lee el dialogo retro en pantalla.");
    
    // Centramos el texto verticalmente pero tirado a la izquierda para que sea un diálogo real
    lv_obj_align(label_frase, LV_ALIGN_LEFT_MID, 15, 0);
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

    // Reemplaza la parte final de clock_screen_update por esta:
    char buf_fecha[50];
    // Le añadimos un separador o formato limpio estilo RPG
    snprintf(buf_fecha, sizeof(buf_fecha), "%s | %d de %s",
             dias[t->tm_wday],
             t->tm_mday,
             meses[t->tm_mon]);
    lv_label_set_text(label_fecha, buf_fecha);
}