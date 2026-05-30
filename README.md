# Proyecto02 
#### Desarrollado por:
#### Keren Fuentes Guevara 
#### Isaac Rojas Robles

Herramienta CLI + librería en C para **indexar archivos JSON** en un formato binario (`.jnx`) y luego buscar nodos por *path* usando expresiones regulares (POSIX ERE), extrayendo los valores directamente del JSON original por offsets.

El binario se llama **`jqindex`** y soporta:

- `build <archivo.json>`: genera `archivo.jnx`.
- `search <archivo.json> <patrón>`: carga `archivo.jnx`, filtra paths que matcheen con `<patrón>` y devuelve un arreglo JSON con `{ "path", "value" }`.

## Requisitos

- Compilación: `gcc` (C11) + `make`.
- Tests de integración: `bash` (ejecuta `tests/run_tests.sh`).


## Compilar

Desde la carpeta del proyecto (la que contiene el `Makefile`):

```bash
cd Proyecto02
make
```

Esto genera el ejecutable `jqindex` en esa misma carpeta.

## Uso (CLI)

### 1) Construir el índice

```bash
./jqindex build tests/data/simple.json
```

Salida (stderr) esperada: cantidad de entradas indexadas y el nombre del `.jnx` generado.

### 2) Buscar por patrón 

```bash
./jqindex search tests/data/simple.json '^\$\.name$'
```

Imprime en **stdout** un arreglo JSON como:

```json
[
	{ "path": "$.name", "value": "Alice" }
]
```


## Tests

Desde `Proyecto02/`:

```bash
make test
```

Comandos útiles:

```bash
make test-unit   # tests unitarios (binario: tests/unit_tests)
make test-int    # tests de integración (script: tests/run_tests.sh)
make clean       # limpia build/, jqindex, unit_tests y .jnx generados en tests/data/
```
