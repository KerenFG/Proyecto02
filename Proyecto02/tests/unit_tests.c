
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test_framework.h"

/* Headers from src/ — Makefile adds -Isrc */
#include "json_parser.h"
#include "jnx_io.h"
#include "json_extractor.h"
#include "regex_engine.h"



#define MAX_EVENTS 64

typedef struct {
    json_event_t items[MAX_EVENTS];
    int          count;
} event_list_t;

static void collect_event(const json_event_t *ev, void *ud)
{
    event_list_t *list = (event_list_t *)ud;
    if (list->count < MAX_EVENTS)
        list->items[list->count++] = *ev;
}

/*
 * parse_str — run the parser over an in-memory JSON string.
 * Returns the collected events via an event_list_t on the stack.
 */
static event_list_t parse_str(const char *json)
{
    event_list_t list;
    memset(&list, 0, sizeof(list));

    FILE *f = tmpfile();
    if (!f) return list;

    fwrite(json, 1, strlen(json), f);
    rewind(f);

    json_parser_t *p = json_parser_create(collect_event, &list);
    if (p) {
        json_parser_run(p, f);
        json_parser_free(p);
    }
    fclose(f);
    return list;
}

/* Temp path used by jnx_io roundtrip tests */
#define TMP_JNX "/tmp/jqindex_unit_test.jnx"


TEST(parser_simple_number)
{
    event_list_t ev = parse_str("{\"x\":1}");
    ASSERT_EQ(ev.count, 2);
    ASSERT_STR(ev.items[0].path, "$.x");
    ASSERT_EQ(ev.items[0].type, JNX_TYPE_NUMBER);
    ASSERT_LONG_EQ(ev.items[0].offset_start, 5);
    ASSERT_LONG_EQ(ev.items[0].offset_end,   6);
    ASSERT_STR(ev.items[1].path, "$");
    ASSERT_EQ(ev.items[1].type, JNX_TYPE_OBJECT);
    ASSERT_LONG_EQ(ev.items[1].offset_start, 0);
    ASSERT_LONG_EQ(ev.items[1].offset_end,   7);
}

\
TEST(parser_string_offsets)
{
    event_list_t ev = parse_str("{\"s\":\"hi\"}");
    ASSERT_EQ(ev.count, 2);
    ASSERT_STR(ev.items[0].path, "$.s");
    ASSERT_EQ(ev.items[0].type, JNX_TYPE_STRING);
    ASSERT_LONG_EQ(ev.items[0].offset_start, 5);
    ASSERT_LONG_EQ(ev.items[0].offset_end,   9);
}


TEST(parser_booleans_and_null)
{
    event_list_t ev = parse_str("{\"a\":true,\"b\":false,\"c\":null}");
    ASSERT_EQ(ev.count, 4);
    ASSERT_EQ(ev.items[0].type, JNX_TYPE_BOOL);
    ASSERT_STR(ev.items[0].path, "$.a");
    ASSERT_EQ(ev.items[1].type, JNX_TYPE_BOOL);
    ASSERT_STR(ev.items[1].path, "$.b");
    ASSERT_EQ(ev.items[2].type, JNX_TYPE_NULL);
    ASSERT_STR(ev.items[2].path, "$.c");
    ASSERT_EQ(ev.items[3].type, JNX_TYPE_OBJECT);
}


TEST(parser_array_paths)
{
    event_list_t ev = parse_str("[1,2,3]");
    ASSERT_EQ(ev.count, 4);
    ASSERT_STR(ev.items[0].path, "$[0]");
    ASSERT_EQ(ev.items[0].type, JNX_TYPE_NUMBER);
    ASSERT_STR(ev.items[1].path, "$[1]");
    ASSERT_STR(ev.items[2].path, "$[2]");
    ASSERT_STR(ev.items[3].path, "$");
    ASSERT_EQ(ev.items[3].type, JNX_TYPE_ARRAY);
}


TEST(parser_empty_containers)
{
    event_list_t ev_obj = parse_str("{}");
    ASSERT_EQ(ev_obj.count, 1);
    ASSERT_STR(ev_obj.items[0].path, "$");
    ASSERT_EQ(ev_obj.items[0].type, JNX_TYPE_OBJECT);
    ASSERT_LONG_EQ(ev_obj.items[0].offset_start, 0);
    ASSERT_LONG_EQ(ev_obj.items[0].offset_end,   2);

    event_list_t ev_arr = parse_str("[]");
    ASSERT_EQ(ev_arr.count, 1);
    ASSERT_EQ(ev_arr.items[0].type, JNX_TYPE_ARRAY);
    ASSERT_LONG_EQ(ev_arr.items[0].offset_end, 2);
}


TEST(parser_nested_paths)
{
    event_list_t ev = parse_str("{\"a\":{\"b\":{\"c\":1}}}");
    ASSERT_EQ(ev.count, 4);
    ASSERT_STR(ev.items[0].path, "$.a.b.c");
    ASSERT_STR(ev.items[1].path, "$.a.b");
    ASSERT_STR(ev.items[2].path, "$.a");
    ASSERT_STR(ev.items[3].path, "$");
}


TEST(parser_array_of_objects)
{
    event_list_t ev = parse_str("{\"arr\":[{\"id\":1},{\"id\":2}]}");
    ASSERT_EQ(ev.count, 6);
    ASSERT_STR(ev.items[0].path, "$.arr[0].id");
    ASSERT_STR(ev.items[1].path, "$.arr[0]");
    ASSERT_STR(ev.items[2].path, "$.arr[1].id");
    ASSERT_STR(ev.items[3].path, "$.arr[1]");
    ASSERT_STR(ev.items[4].path, "$.arr");
    ASSERT_STR(ev.items[5].path, "$");
}


TEST(parser_string_escape)
{
    event_list_t ev = parse_str("{\"k\":\"a\\\"b\"}");
    ASSERT_EQ(ev.count, 2);
    ASSERT_STR(ev.items[0].path, "$.k");
    ASSERT_EQ(ev.items[0].type, JNX_TYPE_STRING);
    ASSERT_LONG_EQ(ev.items[0].offset_start, 5);
    ASSERT_LONG_EQ(ev.items[0].offset_end,   11);
}




static jnx_entry_t make_entry(const char *path, long start, long end, uint8_t type)
{
    jnx_entry_t e;
    memset(&e, 0, sizeof(e));
    strncpy(e.path, path, JNX_MAX_PATH_LEN - 1);
    e.offset_start = start;
    e.offset_end   = end;
    e.type         = type;
    return e;
}

TEST(jnx_write_read_roundtrip)
{
    jnx_entry_t w[3];
    w[0] = make_entry("$",      0,  25, JNX_TYPE_OBJECT);
    w[1] = make_entry("$.name", 8,  15, JNX_TYPE_STRING);
    w[2] = make_entry("$.age",  22, 24, JNX_TYPE_NUMBER);

  
    jnx_writer_t *wr = jnx_writer_open(TMP_JNX, 100);
    ASSERT_NOT_NULL(wr);
    if (!wr) return;

    for (int i = 0; i < 3; i++)
        ASSERT_EQ(jnx_writer_append(wr, &w[i]), 0);

    ASSERT_EQ(jnx_writer_close(wr), 0);

   
    jnx_index_t *idx = jnx_index_load(TMP_JNX);
    ASSERT_NOT_NULL(idx);
    if (!idx) { remove(TMP_JNX); return; }

    ASSERT_EQ((int)idx->count, 3);
    ASSERT_STR(idx->entries[0].path, "$");
    ASSERT_LONG_EQ(idx->entries[0].offset_start, 0);
    ASSERT_LONG_EQ(idx->entries[0].offset_end,   25);
    ASSERT_EQ(idx->entries[0].type, JNX_TYPE_OBJECT);

    ASSERT_STR(idx->entries[1].path, "$.name");
    ASSERT_LONG_EQ(idx->entries[1].offset_start, 8);
    ASSERT_LONG_EQ(idx->entries[1].offset_end,   15);
    ASSERT_EQ(idx->entries[1].type, JNX_TYPE_STRING);

    ASSERT_STR(idx->entries[2].path, "$.age");
    ASSERT_EQ(idx->entries[2].type, JNX_TYPE_NUMBER);

    jnx_index_free(idx);
    remove(TMP_JNX);
}

TEST(jnx_empty_index)
{
    jnx_writer_t *wr = jnx_writer_open(TMP_JNX, 0);
    ASSERT_NOT_NULL(wr);
    if (!wr) return;

    ASSERT_EQ(jnx_writer_close(wr), 0);

    jnx_index_t *idx = jnx_index_load(TMP_JNX);
    ASSERT_NOT_NULL(idx);
    if (!idx) { remove(TMP_JNX); return; }

    ASSERT_EQ((int)idx->count, 0);
    ASSERT_NULL(idx->entries);

    jnx_index_free(idx);
    remove(TMP_JNX);
}

TEST(jnx_bad_magic_returns_null)
{
   
    FILE *f = fopen(TMP_JNX, "wb");
    ASSERT_NOT_NULL(f);
    if (!f) return;
    fputs("NOT_A_JNX_FILE", f);
    fclose(f);

    jnx_index_t *idx = jnx_index_load(TMP_JNX);
    ASSERT_NULL(idx);

    remove(TMP_JNX);
}

TEST(jnx_path_preserved_exactly)
{
   
    char long_path[JNX_MAX_PATH_LEN];
    memset(long_path, 'a', JNX_MAX_PATH_LEN - 1);
    long_path[JNX_MAX_PATH_LEN - 1] = '\0';

    jnx_entry_t e = make_entry(long_path, 0, 1, JNX_TYPE_STRING);

    jnx_writer_t *wr = jnx_writer_open(TMP_JNX, 10);
    ASSERT_NOT_NULL(wr);
    if (!wr) return;

    jnx_writer_append(wr, &e);
    jnx_writer_close(wr);

    jnx_index_t *idx = jnx_index_load(TMP_JNX);
    ASSERT_NOT_NULL(idx);
    if (!idx) { remove(TMP_JNX); return; }

    ASSERT_EQ((int)idx->count, 1);
    ASSERT_STR(idx->entries[0].path, long_path);

    jnx_index_free(idx);
    remove(TMP_JNX);
}



TEST(extractor_basic_range)
{
   
    const char *content = "{\"name\":\"Alice\"}";
    FILE *f = tmpfile();
    ASSERT_NOT_NULL(f);
    if (!f) return;
    fwrite(content, 1, strlen(content), f);

   
    jnx_entry_t e = make_entry("$.name", 8, 15, JNX_TYPE_STRING);
    char buf[32];
    long n = json_extract_entry(f, &e, buf, sizeof(buf));

    ASSERT_LONG_EQ(n, 7);              
    ASSERT_STR(buf, "\"Alice\"");

    fclose(f);
}

TEST(extractor_number_range)
{
  
    const char *content = "{\"age\":30}";
    FILE *f = tmpfile();
    ASSERT_NOT_NULL(f);
    if (!f) return;
    fwrite(content, 1, strlen(content), f);

    jnx_entry_t e = make_entry("$.age", 7, 9, JNX_TYPE_NUMBER);
    char buf[16];
    long n = json_extract_entry(f, &e, buf, sizeof(buf));

    ASSERT_LONG_EQ(n, 2);
    ASSERT_STR(buf, "30");

    fclose(f);
}

TEST(extractor_full_object)
{
 
    const char *content = "{\"x\":1}";
    FILE *f = tmpfile();
    ASSERT_NOT_NULL(f);
    if (!f) return;
    fwrite(content, 1, strlen(content), f);

    jnx_entry_t e = make_entry("$", 0, 7, JNX_TYPE_OBJECT);
    char buf[32];
    long n = json_extract_entry(f, &e, buf, sizeof(buf));

    ASSERT_LONG_EQ(n, 7);
    ASSERT_STR(buf, "{\"x\":1}");

    fclose(f);
}

TEST(extractor_buffer_too_small)
{
    const char *content = "{\"x\":1}";
    FILE *f = tmpfile();
    ASSERT_NOT_NULL(f);
    if (!f) return;
    fwrite(content, 1, strlen(content), f);

    jnx_entry_t e = make_entry("$", 0, 7, JNX_TYPE_OBJECT);
    char buf[4];   /* too small for 7 bytes + '\0' */
    long n = json_extract_entry(f, &e, buf, sizeof(buf));

    ASSERT_LONG_EQ(n, -1);
    fclose(f);
}

TEST(extractor_empty_range)
{
    const char *content = "{}";
    FILE *f = tmpfile();
    ASSERT_NOT_NULL(f);
    if (!f) return;
    fwrite(content, 1, strlen(content), f);

  
    jnx_entry_t e = make_entry("$", 5, 5, JNX_TYPE_OBJECT);
    char buf[8] = "XXXX";
    long n = json_extract_entry(f, &e, buf, sizeof(buf));

    ASSERT_LONG_EQ(n, 0);
    ASSERT_EQ(buf[0], '\0');

    fclose(f);
}


static jnx_index_t *make_index(const char **paths, int n)
{
    jnx_index_t *idx = malloc(sizeof(*idx));
    if (!idx) return NULL;

    idx->count   = (uint64_t)n;
    idx->entries = calloc((size_t)n, sizeof(jnx_entry_t));
    if (!idx->entries) { free(idx); return NULL; }

    for (int i = 0; i < n; i++) {
        strncpy(idx->entries[i].path, paths[i], JNX_MAX_PATH_LEN - 1);
        idx->entries[i].type         = JNX_TYPE_STRING;
        idx->entries[i].offset_start = (long)i * 10;
        idx->entries[i].offset_end   = (long)i * 10 + 5;
    }
    return idx;
}

TEST(regex_exact_match)
{
    const char *paths[] = { "$", "$.name", "$.age", "$.items[0]" };
    jnx_index_t *idx = make_index(paths, 4);
    ASSERT_NOT_NULL(idx);
    if (!idx) return;

    regex_result_t *r = regex_search(idx, "^\\$\\.name$");
    ASSERT_NOT_NULL(r);
    if (!r) { jnx_index_free(idx); return; }

    ASSERT_EQ((int)r->count, 1);
    ASSERT_STR(r->entries[0]->path, "$.name");

    regex_result_free(r);
    jnx_index_free(idx);
}

TEST(regex_partial_match)
{
    const char *paths[] = { "$", "$.items[0]", "$.items[1]", "$.other" };
    jnx_index_t *idx = make_index(paths, 4);
    ASSERT_NOT_NULL(idx);
    if (!idx) return;

  
    regex_result_t *r = regex_search(idx, "items");
    ASSERT_NOT_NULL(r);
    if (!r) { jnx_index_free(idx); return; }

    ASSERT_EQ((int)r->count, 2);

    regex_result_free(r);
    jnx_index_free(idx);
}

TEST(regex_no_match_returns_empty_not_null)
{
    const char *paths[] = { "$.a", "$.b" };
    jnx_index_t *idx = make_index(paths, 2);
    ASSERT_NOT_NULL(idx);
    if (!idx) return;

    regex_result_t *r = regex_search(idx, "^\\$\\.z$");
    ASSERT_NOT_NULL(r);        
    if (!r) { jnx_index_free(idx); return; }

    ASSERT_EQ((int)r->count, 0);
    ASSERT_NULL(r->entries);

    regex_result_free(r);
    jnx_index_free(idx);
}

TEST(regex_bad_pattern_returns_null)
{
    const char *paths[] = { "$.x" };
    jnx_index_t *idx = make_index(paths, 1);
    ASSERT_NOT_NULL(idx);
    if (!idx) return;


    regex_result_t *r = regex_search(idx, "[invalid");
    ASSERT_NULL(r);

    jnx_index_free(idx);
}

TEST(regex_empty_index)
{
    jnx_index_t idx = { .entries = NULL, .count = 0 };
    regex_result_t *r = regex_search(&idx, ".*");
    ASSERT_NOT_NULL(r);
    if (!r) return;

    ASSERT_EQ((int)r->count, 0);
    regex_result_free(r);
}

TEST(regex_match_all)
{
    const char *paths[] = { "$", "$.a", "$.b", "$.a.c" };
    jnx_index_t *idx = make_index(paths, 4);
    ASSERT_NOT_NULL(idx);
    if (!idx) return;

    regex_result_t *r = regex_search(idx, ".*");
    ASSERT_NOT_NULL(r);
    if (!r) { jnx_index_free(idx); return; }

    ASSERT_EQ((int)r->count, 4);

    regex_result_free(r);
    jnx_index_free(idx);
}

TEST(regex_alternation)
{
    const char *paths[] = { "$.name", "$.age", "$.email" };
    jnx_index_t *idx = make_index(paths, 3);
    ASSERT_NOT_NULL(idx);
    if (!idx) return;

    regex_result_t *r = regex_search(idx, "name|email");
    ASSERT_NOT_NULL(r);
    if (!r) { jnx_index_free(idx); return; }

    ASSERT_EQ((int)r->count, 2);

    regex_result_free(r);
    jnx_index_free(idx);
}



int main(void)
{
    SUITE("json_parser");
    RUN(parser_simple_number);
    RUN(parser_string_offsets);
    RUN(parser_booleans_and_null);
    RUN(parser_array_paths);
    RUN(parser_empty_containers);
    RUN(parser_nested_paths);
    RUN(parser_array_of_objects);
    RUN(parser_string_escape);

    SUITE("jnx_io");
    RUN(jnx_write_read_roundtrip);
    RUN(jnx_empty_index);
    RUN(jnx_bad_magic_returns_null);
    RUN(jnx_path_preserved_exactly);

    SUITE("json_extractor");
    RUN(extractor_basic_range);
    RUN(extractor_number_range);
    RUN(extractor_full_object);
    RUN(extractor_buffer_too_small);
    RUN(extractor_empty_range);

    SUITE("regex_engine");
    RUN(regex_exact_match);
    RUN(regex_partial_match);
    RUN(regex_no_match_returns_empty_not_null);
    RUN(regex_bad_pattern_returns_null);
    RUN(regex_empty_index);
    RUN(regex_match_all);
    RUN(regex_alternation);

    SUMMARY();
}
