

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "../include/libjsonindex.h"

/* ── Códigos de salida ───────────────────────────────────────────────────── */

#define EXIT_OK      0
#define EXIT_USAGE   1
#define EXIT_RUNTIME 2

/* ── Helpers de sistema de archivos ─────────────────────────────────────── */

/* Retorna 1 si el archivo existe, 0 si no. */
static int file_exists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

/* Retorna 1 si el .json es más nuevo que el .jnx (índice desactualizado).
 * También retorna 1 si el .jnx no existe. */
static int is_index_stale(const char *json_path, const char *jnx_path)
{
    struct stat json_st, jnx_st;

    if (stat(jnx_path, &jnx_st) != 0)  return 1;   /* .jnx no existe */
    if (stat(json_path, &json_st) != 0) return 0;   /* no se puede revisar */

    return json_st.st_mtime > jnx_st.st_mtime;
}




static int derive_jnx_path(const char *json_path, char *out_buf, size_t out_size)
{
    /* Encuentra el inicio del nombre de archivo (después del último '/' o '\') */
    const char *base = json_path;
    for (const char *p = json_path; *p; p++) {
        if (*p == '/' || *p == '\\') base = p + 1;
    }

    /* Busca el último '.' en el nombre de archivo */
    const char *dot = strrchr(base, '.');

    if (dot) {
        size_t prefix_len = (size_t)(dot - json_path);
        if (prefix_len + 5 > out_size) return -1;
        memcpy(out_buf, json_path, prefix_len);
        memcpy(out_buf + prefix_len, ".jnx", 5);
    } else {
        if (strlen(json_path) + 5 > out_size) return -1;
        snprintf(out_buf, out_size, "%s.jnx", json_path);
    }
    return 0;
}



static void print_version(void)
{
    fprintf(stderr, "jqindex v%d\n", JNX_VERSION);
}

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "Uso:\n"
        "  %s build  <archivo.json>\n"
        "  %s search <archivo.json> <patrón>\n"
        "\n"
        "Subcomandos:\n"
        "  build   Parsea <archivo.json> y escribe el índice en <archivo.jnx>.\n"
        "  search  Carga <archivo.jnx>, busca paths que coincidan con <patrón>\n"
        "          (POSIX ERE), extrae los fragmentos e imprime un arreglo JSON.\n"
        "\n"
        "Ejemplos de patrones:\n"
        "  \"\\$\\.nombre\"           campo 'nombre' en la raíz\n"
        "  \"\\$\\.usuarios\\[\"       cualquier elemento del arreglo 'usuarios'\n"
        "  \"\\$\\..*\\.email\"        campo 'email' a cualquier profundidad\n"
        "  \"\\$\\.items\\[[0-9]+\\]\"  todos los elementos del arreglo 'items'\n"
        "\n"
        "Salida:\n"
        "  build   imprime la cantidad de entradas en stderr.\n"
        "  search  imprime el arreglo JSON en stdout; la cantidad en stderr.\n"
        "\n"
        "Opciones:\n"
        "  --version  muestra la versión y termina\n"
        "  --help     muestra esta ayuda y termina\n",
        prog, prog);
}



static int cmd_build(const char *json_path)
{
    if (!file_exists(json_path)) {
        fprintf(stderr, "error: archivo no encontrado: '%s'\n", json_path);
        return EXIT_RUNTIME;
    }

    char jnx_path[4096];
    if (derive_jnx_path(json_path, jnx_path, sizeof(jnx_path)) != 0) {
        fprintf(stderr, "error: path demasiado largo: '%s'\n", json_path);
        return EXIT_RUNTIME;
    }

    int n = jnx_build(json_path, jnx_path);
    if (n < 0) {
        fprintf(stderr, "error: falló la indexación de '%s'\n", json_path);
        return EXIT_RUNTIME;
    }

    fprintf(stderr, "%d entradas indexadas → %s\n", n, jnx_path);
    return EXIT_OK;
}

static int cmd_search(const char *json_path, const char *pattern)
{
    if (!file_exists(json_path)) {
        fprintf(stderr, "error: archivo no encontrado: '%s'\n", json_path);
        return EXIT_RUNTIME;
    }

    char jnx_path[4096];
    if (derive_jnx_path(json_path, jnx_path, sizeof(jnx_path)) != 0) {
        fprintf(stderr, "error: path demasiado largo: '%s'\n", json_path);
        return EXIT_RUNTIME;
    }

    if (!file_exists(jnx_path)) {
        fprintf(stderr,
                "error: índice no encontrado: '%s'\n"
                "       ejecuta: jqindex build %s\n",
                jnx_path, json_path);
        return EXIT_RUNTIME;
    }

    /* Avisa si el índice podría estar desactualizado. */
    if (is_index_stale(json_path, jnx_path)) {
        fprintf(stderr,
                "advertencia: índice desactualizado: '%s' es más nuevo que'%s'\n"
                "         los resultados podrían estar desactualizados\n",
                json_path, jnx_path);
    }

    int n = jnx_search(json_path, jnx_path, pattern, stdout);
    if (n < 0) {
        fprintf(stderr, "error: falló la búsqueda\n");
        return EXIT_RUNTIME;
    }

    fprintf(stderr, "%d coincidencia(s)\n", n);
    return EXIT_OK;
}



int main(int argc, char *argv[])
{
    /* Opciones globales antes del subcomando. */
    if (argc >= 2) {
        if (strcmp(argv[1], "--help")    == 0 ||
            strcmp(argv[1], "-h")        == 0 ||
            strcmp(argv[1], "help")      == 0) {
            print_usage(argv[0]);
            return EXIT_OK;
        }
        if (strcmp(argv[1], "--version") == 0 ||
            strcmp(argv[1], "-v")        == 0) {
            print_version();
            return EXIT_OK;
        }
    }

    if (argc < 3) {
        print_usage(argv[0]);
        return EXIT_USAGE;
    }

    const char *subcmd    = argv[1];
    const char *json_path = argv[2];

    if (strcmp(subcmd, "build") == 0) {
        if (argc != 3) {
            fprintf(stderr, "error: 'build' recibe exactamente un argumento\n\n");
            print_usage(argv[0]);
            return EXIT_USAGE;
        }
        return cmd_build(json_path);
    }

    if (strcmp(subcmd, "search") == 0) {
        if (argc != 4) {
            fprintf(stderr, "error: 'search' recibe exactamente dos argumentos\n\n");
            print_usage(argv[0]);
            return EXIT_USAGE;
        }
        return cmd_search(json_path, argv[3]);
    }

    fprintf(stderr, "error: subcomando desconocido '%s'\n\n", subcmd);
    print_usage(argv[0]);
    return EXIT_USAGE;
}
