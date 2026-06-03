#include "alarma.h"
#include <stdbool.h>

static int alarma_hora = 0;
static int alarma_min = 0;
static bool alarma_activa = false;

void alarma_init(void)
{
    alarma_hora = 0;
    alarma_min = 0;
    alarma_activa = false;
}

void alarma_set(int hora, int minuto)
{
    alarma_hora = hora % 24;
    alarma_min = minuto % 60;
    alarma_activa = true;
}

void alarma_clear(void)
{
    alarma_activa = false;
}

void alarma_get(int *hora, int *minuto)
{
    if (hora)
        *hora = alarma_hora;
    if (minuto)
        *minuto = alarma_min;
}

bool alarma_es_activa(void)
{
    return alarma_activa;
}

bool alarma_debe_sonar(const struct tm *t)
{
    if (!alarma_activa || t == NULL)
        return false;

    return (t->tm_hour == alarma_hora && t->tm_min == alarma_min && t->tm_sec == 0);
}
