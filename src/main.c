#include "lvgl/lvgl.h"
#include "screens/clock_screen.h"
#include <unistd.h>
 
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
 
    lv_init();
 
    lv_sdl_window_create(1024, 600);
    lv_sdl_mouse_create();
 
    // ── Arranca la pantalla del reloj ──
    clock_screen_create();
 
    while (1) {
        lv_timer_handler();
        usleep(5000);
    }
 
    return 0;
}
 
