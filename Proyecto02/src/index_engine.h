

#ifndef INDEX_ENGINE_H
#define INDEX_ENGINE_H

/* Construye el índice .jnx a partir de un archivo .json.
   Lee el .json, corre el parser y escribe cada entrada directo al disco.
   No mantiene el índice completo en memoria durante la construcción.
   Retorna la cantidad de entradas escritas o -1 si hay error. */
int index_engine_build(const char *json_path, const char *jnx_path);

#endif /* INDEX_ENGINE_H */
