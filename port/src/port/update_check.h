// The update check and the one-click update (simgolf mod, card task-80). Native build only.
// One request per start for the newest release (no data sent but the request itself), on a worker thread so
// the menu is never held up; the answer fills the UpdState the start menu shows. "Update now" downloads the
// release zip, checks its SHA-256 and hands over to sgupdate.exe (port/src/tools/sgupdate.c).
#pragma once

#include "port/ui/ui_updates.h"

// Starts the check once per run (later calls do nothing). With Options > "Check for updates" off, or the
// running build marked "dev" and no test feed, no network is touched at all. Logs "update: ..." lines.
// SIMGOLF_UPDATE_FEED=<file or http url> replaces the GitHub address (tests), SIMGOLF_UPDATE_NO_APPLY=1
// makes "Update now" stop after the download and the hash check.
void upd_check_start(UpdState *u);

// "Update now": download the zip and its .sha256 beside the game, verify, start sgupdate.exe and quit the
// game. Runs on a worker thread; progress and failure are in u->busy / u->status_text.
void upd_install_start(UpdState *u);
