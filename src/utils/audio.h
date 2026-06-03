#ifndef AUDIO_H
#define AUDIO_H

// Funciones de audio reutilizables
// Comentarios esenciales en español: describen responsabilidad y contratos simples.

#ifdef __cplusplus
extern "C"
{
#endif

    // Devuelve la duración estimada (segundos) de un MP3. Retorna 180 si no se puede determinar.
    int obtener_duracion_mp3(const char *ruta_archivo);

    // Extrae la carátula APIC de un MP3 con ID3v2. Guarda JPEG en ruta_salida_jpg.
    // Retorna 1 si se extrajo correctamente, 0 en caso contrario.
    int extraer_caratula_mp3(const char *ruta_mp3, const char *ruta_salida_jpg);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_H
