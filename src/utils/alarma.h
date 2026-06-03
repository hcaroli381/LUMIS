#ifndef ALARMA_H
#define ALARMA_H

#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

    // Módulo de alarma: encapsula hora confirmada y estado activo.

    // Inicializa la alarma con valores por defecto.
    void alarma_init(void);

    // Configura la alarma y la activa.
    void alarma_set(int hora, int minuto);

    // Desactiva la alarma.
    void alarma_clear(void);

    // Obtiene la hora y minuto confirmados.
    void alarma_get(int *hora, int *minuto);

    // Retorna true si la alarma está activa.
    bool alarma_es_activa(void);

    // Comprueba si la alarma debe sonar para el tiempo proporcionado.
    // Retorna true si la alarma está activa y coincide con la hora y minuto del struct tm.
    bool alarma_debe_sonar(const struct tm *t);

#ifdef __cplusplus
}
#endif

#endif // ALARMA_H
