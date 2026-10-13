// Mod manager model (simgolf mod, card task-22): everything the in-game Mods screen decides, with no
// drawing and no game code, so the unit tests (tests/ui/test_modmgr.c) and the screen use one path.
//
// The model holds a DRAFT of mods/load_order.toml (which profile is active, every profile's list) and
// resolves it with the platform's own resolver (mods_resolve): dependencies, conflicts, order and the
// plain messages come from there, never from the screen. Nothing reaches the disk until
// modmgr_apply, which writes load_order.toml exactly as tools/modctl does (loadorder_write). Whether a
// change needs a restart is the difference between the draft's resolved set and the set the running
// game loaded (given at open).
#ifndef SIMGOLF_MODMGR_H
#define SIMGOLF_MODMGR_H

#include "mods/mods_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ModMgr ModMgr;

// mods_dir: the mods folder. running/nrunning: the ids the game loaded, in load order (NULL: nothing is
// running, e.g. a tool). 1 ok. Reads the mods.ini switches from the store like the game does at start.
ModMgr *modmgr_open(const char *mods_dir, const char *const *running, int nrunning);
void modmgr_close(ModMgr *m);
// Reads everything again from disk, dropping the draft (Revert).
void modmgr_reload(ModMgr *m);

// ------------------------------------------------------------------ the mods

typedef struct {
  int on;             // in the draft profile's list
  int pos;            // index in that list, -1 when off
  int load_pos;       // 0-based place in the resolved load order, -1 when not loaded
  int running;        // the running game loaded it
  int broken;         // mod.toml unusable
  int conflict;       // it overrides, or is overridden by, another mod on some key
  int missing_dep;    // a required dependency is missing, off or the wrong version
  int incompatible;   // dropped, or would drop another, because of incompatibilities
  int auto_enabled;   // loads only because another mod needs it
  int changes_saves;  // mod.toml: changes_saves = true
  int needs_restart;  // loads (or not, or in another place) differently from the running game
  long added;         // mod.toml modification time (recently added sort)
} ModInfo;

int modmgr_count(const ModMgr *m);
const ModManifest *modmgr_manifest(const ModMgr *m, int i);
const ModInfo *modmgr_info(const ModMgr *m, int i);
int modmgr_find(const ModMgr *m, const char *id);   // index or -1
const char *modmgr_author(const ModMgr *m, int i, char *out, size_t n);   // "Ann, Bob" or ""

typedef struct {
  int level;          // MD_NOTE / MD_WARN / MD_ERROR
  char code[24];
  char msg[320];
} ModNote;
// The resolver's messages about one mod (conflicts, missing dependencies, ...), most serious first.
int modmgr_notes(const ModMgr *m, int i, ModNote *out, int max);
// Other mods' conflicts that involve mod i: one line per key ("economy.x: A sets 1, B sets 2; B wins").
int modmgr_conflict_lines(const ModMgr *m, int i, char (*out)[320], int max);

// ------------------------------------------------------------------ view: search, filter, sort

enum { MM_SORT_NAME, MM_SORT_AUTHOR, MM_SORT_RECENT, MM_SORT_LOAD, MM_SORT_ENABLED, MM_SORT__COUNT };
extern const char *const modmgr_sort_names[MM_SORT__COUNT];

typedef struct {
  char search[64];      // words, all must appear in name, id, author, description or tags (no case)
  int sort;             // MM_SORT_*
  int only_enabled, only_disabled, only_conflicts;
  char tag[24];         // "" or a tag from mod.toml (no case)
} ModView;

// Indexes of the mods that pass the filters, in the sort order. Returns the count (up to max).
int modmgr_view(const ModMgr *m, const ModView *v, int *out, int max);
// Every tag used by an installed mod, sorted, up to max; returns the count.
int modmgr_tags(const ModMgr *m, char (*out)[24], int max);

// ------------------------------------------------------------------ edits (draft only)

// Turn a mod on (appended last) or off. Dependencies the resolver turns on by itself show as auto_enabled.
int modmgr_set_enabled(ModMgr *m, int i, int on);
// Move an enabled mod in the profile list: by delta places, or to an absolute place. 1 if it moved.
int modmgr_move(ModMgr *m, int i, int delta);
int modmgr_move_to(ModMgr *m, int i, int pos);

// ------------------------------------------------------------------ profiles

int modmgr_profile_count(const ModMgr *m);
const char *modmgr_profile_name(const ModMgr *m, int k);   // "vanilla" is always last-known to exist
const char *modmgr_profile(const ModMgr *m);                // the draft's active profile
int modmgr_profile_readonly(const ModMgr *m, const char *name);   // vanilla
// Valid names: 1-32 of letters, digits, '_' and '-' (spaces become '_'). Returns 1 when usable;
// writes the cleaned name to out (n bytes).
int modmgr_clean_name(const char *in, char *out, size_t n);
int modmgr_profile_use(ModMgr *m, const char *name);              // switch the draft to it
// Create (empty, or a copy of the current list when copy=1) and switch to it. 0 with reason in err.
int modmgr_profile_create(ModMgr *m, const char *name, int copy, char *err, size_t en);
int modmgr_profile_duplicate(ModMgr *m, char *err, size_t en);    // "<name>_copy", then switch to it
int modmgr_profile_rename(ModMgr *m, const char *to, char *err, size_t en);
int modmgr_profile_delete(ModMgr *m, const char *name, char *err, size_t en);   // switches to vanilla if it was active

// ------------------------------------------------------------------ apply

int modmgr_dirty(const ModMgr *m);          // the draft differs from load_order.toml on disk
// Anything in the draft resolves differently from what the running game loaded.
int modmgr_restart_needed(const ModMgr *m);
// Writes load_order.toml (the draft) next to the mods; the draft is then "on disk". 1 ok.
int modmgr_apply(ModMgr *m, char *err, size_t en);
int modmgr_error(const ModMgr *m, char *out, size_t n);   // load_order.toml parse error, "" when fine
int modmgr_resolution_errors(const ModMgr *m);            // error-level messages in the draft's resolution

// A copy of the draft's list for the selected profile (ids in order), for the tests and the log.
int modmgr_list(const ModMgr *m, const char **ids, int max);

#ifdef __cplusplus
}
#endif
#endif  // SIMGOLF_MODMGR_H
