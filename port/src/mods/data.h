// Mod data layer (simgolf mod, task-19): the game's numbers and tables as
// moddable data. Plain C shared by the game (data_runtime.c), the tool
// (modctl_core data ...) and the tests. docs/modding/DATA.md.
//
// A table is defined by data/tables/<name>.json: a JSON Schema for one row
// plus "x-table" (row ids, where the rows live in golf.exe's image) and
// "x-image" / "x-code" on fields (where each value lives). Base rows are read
// from the game itself (its loaded image, or golf.bin offline), so no game
// data is stored in this repository; values the game keeps as constants in
// its code come from the definition's rows and are checked against the code
// at run time. Loaded mods then change rows in load order with the files
// mods/<id>/data/<table>.json:
//
//   { "set":    { "snack_bar.cost": 75 },          exclusive: the later mod wins
//     "add":    { "*.cost": 10 },                  numbers combine with other mods
//     "mul":    { "snack_bar.income": 1.5 },
//     "min" / "max": { "row.field": n },
//     "append": { "row.list_field": [ ... ] },
//     "merge":  { "row": { "field": value } },     new id: a new row (extensible tables)
//     "replace":{ "row": { ... whole row ... } },
//     "remove": [ "row" ] }
//
// Every change is registered with the mod platform's conflict registry
// (mods_touch, kind "data", key "table.row.field" or "table.row").

#ifndef SIMGOLF_MODS_DATA_H
#define SIMGOLF_MODS_DATA_H

#include <stdint.h>

#include "mods/json.h"
#include "mods/mods_core.h"

#ifdef __cplusplus
extern "C" {
#endif

// Where base values live.
enum { DB_NONE, DB_IMAGE, DB_CODE };
// Image field types.
enum { DI_U8, DI_I8, DI_U16, DI_I16, DI_U32, DI_I32, DI_STR, DI_STRPTR };

// Access to golf.exe's image: a pointer to len bytes at a virtual address,
// or NULL when it is outside the image. The game passes its own memory;
// tools pass golf.bin (base 0x400000).
typedef struct {
  unsigned char *(*at)(void *ctx, uint32_t addr, uint32_t len);
  void *ctx;
} DataImage;

typedef struct {
  char *name;
  JsonVal *schema;   // the field's subschema (points into the definition)
  int bind;          // DB_*
  int itype;         // DI_* for DB_IMAGE
  uint32_t offset, len;
  int readonly;      // "readOnly": true, mods cannot change it
  int integer;       // "type": "integer"
} DataField;

typedef struct {
  char *name;        // file name without .json
  char *path;
  char *title;
  JsonVal *def;      // the whole definition (also the row schema)
  DataField *f;
  int nf;
  int has_image;
  uint32_t image_base, image_stride;
  int extensible;    // mods may add and remove rows
  int single;        // one row; "field" may stand for "<row>.field"
  // Rows: the first nfixed come from the definition (in the image order).
  int nrows, cap;
  char **row_id;
  JsonVal **base;    // base values (object), NULL for rows added by mods
  JsonVal **row;     // values after the mods (object), NULL when removed
  char **row_mod;    // the mod that added the row (NULL for base rows)
  int nfixed;
  unsigned char *touched;   // [row * nf + field]: a mod changed it
  unsigned char *row_touched;
} DataTable;

typedef struct {
  DataTable *t;
  int n;
  ModDiags diags;    // problems, every one with "file:line:" where it applies
  int changes;       // field values changed by mods
} DataSet;

// Reads every data/tables/*.json definition (name order). Problems go to
// ds->diags (code "data_def"). Returns the number of tables.
int data_load_defs(DataSet *ds, const char *tables_dir);
// Fills base rows: image fields from img (NULL: left out), code fields from
// the definition's rows.
void data_build_base(DataSet *ds, const DataImage *img);

// Registry callback: the game passes mods_touch; returns as mods_touch.
typedef int (*data_touch_fn)(void *ctx, const char *mod_id, const char *key, const char *op);
// Applies <mod_dir>/data/*.json of one mod (call in load order). Errors and
// warnings go to ds->diags with the mod id; bad changes are skipped.
// Returns the number of changes applied.
int data_apply_mod(DataSet *ds, const char *mod_id, const char *mod_dir, data_touch_fn touch, void *ctx);
// Applies one data file (tests, tools): table is the file's table name.
int data_apply_file(DataSet *ds, const char *mod_id, const char *table, const char *path, data_touch_fn touch,
                    void *ctx);

// Writes the image fields that a mod changed back into the image. log gets
// one line per value written. Returns the number of values written.
typedef void (*data_log_fn)(void *ctx, const char *line);
int data_write_image(DataSet *ds, const DataImage *img, data_log_fn log, void *ctx);

// One line per value the mods changed ("table.row.field = new (was old)"),
// added and removed rows. Returns the number of lines.
int data_report_changes(const DataSet *ds, data_log_fn log, void *ctx);

DataTable *data_table(const DataSet *ds, const char *name);
int data_row_index(const DataTable *t, const char *id);     // -1: none (or removed)
int data_field_index(const DataTable *t, const char *name);
// A row's value of a field (NULL: no value).
const JsonVal *data_value(const DataTable *t, int row, int field);

// {"table": name, "title": ..., "rows": {id: {...}}}; effective: after mods.
JsonVal *data_table_json(const DataTable *t, int effective);
// Coverage line per table: bindings and row counts (for DATA.md and reports).
JsonVal *data_coverage_json(const DataSet *ds);

void data_free(DataSet *ds);

// JSON Schema check of one value (the subset in DATA.md). Appends a reason
// to msg and returns 0 when v does not match.
int data_schema_check(const JsonVal *schema, const JsonVal *v, const DataSet *ds, char *msg, size_t msglen);

// golf.bin reader for tools and tests: loads the image file at base.
typedef struct {
  unsigned char *bytes;
  uint32_t base, size;
} DataImageFile;
int data_image_file_open(DataImageFile *f, const char *path, uint32_t base);
void data_image_file_close(DataImageFile *f);
DataImage data_image_of_file(DataImageFile *f);

#ifdef __cplusplus
}
#endif
#endif  // SIMGOLF_MODS_DATA_H
