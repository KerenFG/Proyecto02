

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "json_parser.h"

/* Estado interno del parser. */
struct json_parser {
    json_event_cb  cb;
    void          *userdata;
    FILE          *file;       /* se asigna en json_parser_run              */
};

/* Declaraciones anticipadas (parse_value y parse_object/array se llaman entre sí). */
static void parse_value (json_parser_t *p, char *path, int depth);
static void parse_object(json_parser_t *p, char *path, long offset_start, int depth);
static void parse_array (json_parser_t *p, char *path, long offset_start, int depth);

/* ── Helpers de E/S ─────────────────────────────────────────────────────── */

/* Consume espacios en blanco hasta encontrar un carácter útil. */
static void skip_ws(json_parser_t *p)
{
    int c;
    while ((c = fgetc(p->file)) != EOF) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            ungetc(c, p->file);
            return;
        }
    }
}

/* Consume bytes hasta encontrar la comilla de cierre, respetando escapes.

 */
static void skip_string(json_parser_t *p)
{
    int c;
    while ((c = fgetc(p->file)) != EOF) {
        if (c == '\\') {
            fgetc(p->file);   /* salta el carácter escapado */
        } else if (c == '"') {
            return;
        }
    }
}

/* Lee la clave de un objeto en buf (terminado en NUL).
 */
static void read_key(json_parser_t *p, char *buf, size_t buf_size)
{
    size_t i = 0;
    int c;

    while ((c = fgetc(p->file)) != EOF) {
        if (c == '\\') {
            int esc = fgetc(p->file);
            if (i + 1 < buf_size) {
                switch (esc) {
                    case '"':  buf[i++] = '"';  break;
                    case '\\': buf[i++] = '\\'; break;
                    case '/':  buf[i++] = '/';  break;
                    case 'n':  buf[i++] = '\n'; break;
                    case 'r':  buf[i++] = '\r'; break;
                    case 't':  buf[i++] = '\t'; break;
                    default:   buf[i++] = (char)esc; break;
                    /* \uXXXX: 'u' se guarda; los 4 dígitos hex vienen como chars normales */
                }
            }
        } else if (c == '"') {
            break;
        } else if (i + 1 < buf_size) {
            buf[i++] = (char)c;
        }
    }
    buf[i] = '\0';
}

/* ── Emisión de eventos ─────────────────────────────────────────────────── */

static void emit(json_parser_t *p, const char *path,
                 long offset_start, long offset_end, uint8_t type)
{
    json_event_t ev;
    strncpy(ev.path, path, JNX_MAX_PATH_LEN - 1);
    ev.path[JNX_MAX_PATH_LEN - 1] = '\0';
    ev.offset_start = offset_start;
    ev.offset_end   = offset_end;
    ev.type         = type;
    p->cb(&ev, p->userdata);
}

/* ── Descenso recursivo ─────────────────────────────────────────────────── */


static void parse_object(json_parser_t *p, char *path, long offset_start, int depth)
{
    char key[JNX_MAX_PATH_LEN];
    char child_path[JNX_MAX_PATH_LEN];

    skip_ws(p);
    int c = fgetc(p->file);

    if (c == '}') {
        /* Objeto vacío */
        emit(p, path, offset_start, ftell(p->file), JNX_TYPE_OBJECT);
        return;
    }
    ungetc(c, p->file);

    for (;;) {
        /* Consume la comilla de apertura de la clave. */
        skip_ws(p);
        fgetc(p->file);
        read_key(p, key, sizeof(key));

        /* Consume el ':' */
        skip_ws(p);
        fgetc(p->file);

        /* Construye el path del hijo: "padre.clave" */
        snprintf(child_path, sizeof(child_path), "%s.%s", path, key);

        /* Parsea y emite el valor y sus descendientes. */
        parse_value(p, child_path, depth + 1);

        /* ',' → siguiente par;  '}' → fin del objeto */
        skip_ws(p);
        c = fgetc(p->file);
        if (c == '}') break;
        /* c == ',' : continúa */
    }

    emit(p, path, offset_start, ftell(p->file), JNX_TYPE_OBJECT);
}


static void parse_array(json_parser_t *p, char *path, long offset_start, int depth)
{
    char child_path[JNX_MAX_PATH_LEN];
    int  index = 0;

    skip_ws(p);
    int c = fgetc(p->file);

    if (c == ']') {
        /* Arreglo vacío */
        emit(p, path, offset_start, ftell(p->file), JNX_TYPE_ARRAY);
        return;
    }
    ungetc(c, p->file);

    for (;;) {
        /* Construye el path del hijo: "padre[índice]" */
        snprintf(child_path, sizeof(child_path), "%s[%d]", path, index);

        parse_value(p, child_path, depth + 1);
        index++;

        /* ',' → siguiente elemento;  ']' → fin del arreglo */
        skip_ws(p);
        c = fgetc(p->file);
        if (c == ']') break;
        /* c == ',' : continúa */
    }

    emit(p, path, offset_start, ftell(p->file), JNX_TYPE_ARRAY);
}


static void parse_value(json_parser_t *p, char *path, int depth)
{
    if (depth >= JNX_STACK_DEPTH) {
        fprintf(stderr, "json_parser: profundidad máxima (%d) alcanzada en '%s'\n",
                JNX_STACK_DEPTH, path);
        return;
    }

    skip_ws(p);
    long offset_start = ftell(p->file);
    int  c            = fgetc(p->file);

    if (c == EOF) return;

    switch (c) {

    case '{':
        parse_object(p, path, offset_start, depth);
        break;

    case '[':
        parse_array(p, path, offset_start, depth);
        break;

    case '"':
        skip_string(p);
        emit(p, path, offset_start, ftell(p->file), JNX_TYPE_STRING);
        break;

    case 't':   /* true  */
    case 'f':   /* false */
    {
        int nc;
        while ((nc = fgetc(p->file)) != EOF && isalpha(nc));
        if (nc != EOF) ungetc(nc, p->file);
        emit(p, path, offset_start, ftell(p->file), JNX_TYPE_BOOL);
        break;
    }

    case 'n':   /* null */
    {
        int nc;
        while ((nc = fgetc(p->file)) != EOF && isalpha(nc));
        if (nc != EOF) ungetc(nc, p->file);
        emit(p, path, offset_start, ftell(p->file), JNX_TYPE_NULL);
        break;
    }

    default:    /* número: el primer carácter ya fue consumido ('-' o dígito) */
    {
        int nc;
        while ((nc = fgetc(p->file)) != EOF) {
            /* Para cuando encuentra algo que no puede ser parte de un número JSON. */
            if (nc == ',' || nc == '}' || nc == ']' ||
                nc == ' ' || nc == '\t' || nc == '\n' || nc == '\r') {
                ungetc(nc, p->file);
                break;
            }
        }
        emit(p, path, offset_start, ftell(p->file), JNX_TYPE_NUMBER);
        break;
    }
    }
}

/* ── API pública ────────────────────────────────────────────────────────── */

json_parser_t *json_parser_create(json_event_cb cb, void *userdata)
{
    json_parser_t *p = malloc(sizeof(*p));
    if (!p) return NULL;
    p->cb       = cb;
    p->userdata = userdata;
    p->file     = NULL;
    return p;
}

int json_parser_run(json_parser_t *p, FILE *json_file)
{
    p->file = json_file;
    parse_value(p, "$", 0);

    if (ferror(json_file)) {
        perror("json_parser_run");
        return -1;
    }
    return 0;
}

void json_parser_free(json_parser_t *p)
{
    free(p);
}
