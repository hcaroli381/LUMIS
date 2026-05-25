#include "lvgl/lvgl.h"
#include <unistd.h> // SOLUCIÓN AL USLEEP

// Función de tu pantalla
void mi_pantalla_lumis(void) {
    lv_obj_t * etiqueta = lv_label_create(lv_scr_act());
    lv_label_set_text(etiqueta, "LUMIS: Sistema Iniciado ✨");
    lv_obj_align(etiqueta, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t * boton = lv_btn_create(lv_scr_act());
    lv_obj_align(boton, LV_ALIGN_CENTER, 0, 40);
    
    lv_obj_t * texto_boton = lv_label_create(boton);
    lv_label_set_text(texto_boton, "Hola Hugo");
    lv_obj_center(texto_boton);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    lv_init();

    // SOLUCIÓN AL HAL_INIT (Usamos SDL nativo para el simulador)
    lv_sdl_window_create(1024, 600);
    lv_sdl_mouse_create();

    mi_pantalla_lumis();

    while(1) {
        lv_timer_handler();
        usleep(5000); 
    }

    return 0;
}