// Mod data layer, the game side (simgolf mod, task-19). What the translated
// game code (through translator patches, tools/translate/golf.json), the
// startup and the bridge call. docs/modding/DATA.md.
//
// Generated code reads a moddable constant like this (ghidra_prelude.h):
//   DAT_00571fd4 = DAT_00571fd4 + MDATA("buildings", "snack_bar", "income", 5);
//   iVar23 = MDATA_AT("difficulty", DAT_00822c88, "wage_period", 0x400 / (DAT_00822c88 + 2));
// The literal is the original value: it is used whenever no mod changed the
// field (or the data layer is off), so the game is the original without mods.
// A table whose base value differs from the literal is reported in golf.log.

#ifndef SIMGOLF_MODS_MDATA_H
#define SIMGOLF_MODS_MDATA_H

#ifdef __cplusplus
extern "C" {
#endif

// Startup (port_main.c, after the image and static initializers, before
// WinMain): table definitions, base rows from the live image, the loaded
// mods' data files, changed image fields written back. Off with
// SIMGOLF_DATA=0 or --no-data (then every MDATA is its literal).
void mdata_init(int argc, char **argv);
int mdata_enabled(void);

int mdata_cell(int *cache, const char *table, const char *row, const char *field, int lit);
int mdata_at(int *cache, const char *table, int row, const char *field, int lit);

// Golfer thoughts (FUN_00469b00): begin remembers the text buffer's length,
// end replaces what the routine appended with a personality's line when the
// golfer has a personality with a line for this thought.
void mdata_thought_begin(char *buf);
void mdata_thought_end(int event, int arg, int arg2, int slot, char *buf);

// For the bridge and tools: a table as JSON (effective values), the
// personality of a golfer slot (NULL: none), the startup summary.
char *mdata_table_json(const char *table, int effective);   // malloc, NULL if unknown
const char *mdata_personality_of(int slot);
int mdata_changes(void);
int mdata_errors(void);

#ifdef __cplusplus
}
#endif
#endif  // SIMGOLF_MODS_MDATA_H
