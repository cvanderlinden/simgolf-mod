// Small JSON reader and writer for the mod data layer (simgolf mod, task-19).
// Every value remembers the line it starts on, so validation errors can say
// "file:line". Objects keep their keys in file order. Plain C, no game code.

#ifndef SIMGOLF_MODS_JSON_H
#define SIMGOLF_MODS_JSON_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { JS_NULL, JS_BOOL, JS_NUM, JS_STR, JS_ARR, JS_OBJ };

typedef struct JsonVal JsonVal;
struct JsonVal {
  int type;
  int line;          // 1-based line in the source text (0: made in code)
  int b;             // JS_BOOL
  double n;          // JS_NUM
  char *s;           // JS_STR (NUL-terminated, UTF-8)
  int len;           // JS_ARR, JS_OBJ: element count
  int cap;
  JsonVal **items;   // JS_ARR elements, JS_OBJ values
  char **keys;       // JS_OBJ keys (parallel to items)
};

// Parses text. On failure returns NULL and fills err ("line N: what").
JsonVal *json_parse(const char *text, char *err, size_t errlen);
// Reads and parses a file; err gets "path:line: what" on failure.
JsonVal *json_load(const char *path, char *err, size_t errlen);
void json_free(JsonVal *v);
JsonVal *json_clone(const JsonVal *v);

JsonVal *json_new(int type);
JsonVal *json_num(double n);
JsonVal *json_str(const char *s);
JsonVal *json_bool(int b);
// Object access. json_get returns NULL when missing (keys compare exactly).
JsonVal *json_get(const JsonVal *obj, const char *key);
int json_index(const JsonVal *obj, const char *key);   // -1 when missing
// Sets (replaces) obj[key]; takes ownership of val.
void json_set(JsonVal *obj, const char *key, JsonVal *val);
int json_del(JsonVal *obj, const char *key);           // 1 when removed
void json_push(JsonVal *arr, JsonVal *val);            // takes ownership

int json_is_int(const JsonVal *v);                     // a number with no fraction
const char *json_type_name(int type);                  // "null", "boolean", ...
int json_equal(const JsonVal *a, const JsonVal *b);

// Writes v as indented JSON (2 spaces) into a malloc'd string.
char *json_write(const JsonVal *v);
// One line, no spaces after separators beyond ", " and ": ".
char *json_write_compact(const JsonVal *v);

#ifdef __cplusplus
}
#endif
#endif  // SIMGOLF_MODS_JSON_H
