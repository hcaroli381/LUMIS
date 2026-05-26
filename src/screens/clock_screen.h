#ifndef CLOCK_SCREEN_H
#define CLOCK_SCREEN_H

#include "lvgl/lvgl.h"

// Inicializa y muestra la pantalla del reloj
void clock_screen_create(void);

// Llama esto en el loop principal para actualizar la hora
void clock_screen_update(void);

#endif // CLOCK_SCREEN_H
