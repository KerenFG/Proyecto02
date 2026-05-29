/* test_framework.h — Framework mínimo de pruebas unitarias.
 *
 * Uso:
 *   TEST(mi_caso) { ASSERT(1 + 1 == 2); }
 *
 *   int main(void) {
 *       SUITE("Matemáticas");
 *       RUN(mi_caso);
 *       SUMMARY();
 *   }
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Contadores globales. */
static int _t_run    = 0;
static int _t_passed = 0;
static int _t_failed = 0;


#define ASSERT(cond) do {                                                   \
    _t_run++;                                                               \
    if (cond) {                                                             \
        _t_passed++;                                                        \
    } else {                                                                \
        _t_failed++;                                                        \
        fprintf(stderr, "    FALLA  %s:%d  →  %s\n",                       \
                __FILE__, __LINE__, #cond);                                 \
    }                                                                       \
} while (0)

#define ASSERT_EQ(a, b)     ASSERT((a) == (b))
#define ASSERT_NE(a, b)     ASSERT((a) != (b))
#define ASSERT_LONG_EQ(a,b) ASSERT((long)(a) == (long)(b))
#define ASSERT_STR(a, b)    ASSERT(strcmp((a), (b)) == 0)
#define ASSERT_NOT_NULL(p)  ASSERT((p) != NULL)
#define ASSERT_NULL(p)      ASSERT((p) == NULL)



#define TEST(nombre) static void _test_##nombre(void)

/* Ejecuta una prueba e imprime el resultado en una línea. */
#define RUN(nombre) do {                                                    \
    int _antes = _t_failed;                                                 \
    fprintf(stderr, "  %-40s", #nombre);                                   \
    _test_##nombre();                                                       \
    fprintf(stderr, "%s\n", (_t_failed == _antes) ? "OK" : "FALLA");       \
} while (0)

/* Imprime la sección actual. */
#define SUITE(nombre) fprintf(stderr, "\n[ %s ]\n", (nombre))

/* Imprime el resumen y retorna el código de salida. Usar en main(). */
#define SUMMARY() do {                                                      \
    fprintf(stderr,                                                         \
        "\n%d aserciones: %d pasaron, %d fallaron\n",                      \
        _t_run, _t_passed, _t_failed);                                      \
    return (_t_failed > 0) ? 1 : 0;                                        \
} while (0)

#endif /* TEST_FRAMEWORK_H */
