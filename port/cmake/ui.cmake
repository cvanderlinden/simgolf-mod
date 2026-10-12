# Frosted Mint UI kit (simgolf mod, task-26) and the course HUD (task-30),
# included by native.cmake after the golf target. docs/ui/KIT.md.
#   golf          the kit, the HUD screen, its render side (hud_frosted.c) and game
#                 side (hud_game.c; src/port/golf/hud_hooks.c is the override file)
#   ui_tests      plain C unit tests: tokens, layout, hit tests, routing, icons, PNG
#   ui_gallery    every component at 1920x1080 and 3840x2160 to PNGs, on SDL's
#                 software renderer (no window, no GPU) or a hidden direct3d11 one

set(SIMGOLF_UI_PLAIN src/port/ui/ui_tokens.c src/port/ui/ui_layout.c src/port/ui/ui_hit.c src/port/ui/ui_icons.c src/port/ui/ui_faces.c
  src/port/ui/ui_png.c)
set(SIMGOLF_UI_SDL src/port/ui/ui_draw.c src/port/ui/ui_text.c src/port/ui/ui_kit.c src/port/ui/ui_hud.c src/port/ui/ui_golfers.c src/port/ui/ui_frontend.c src/port/ui/ui_dialogs.c
  src/port/ui/ui_reports.c)

target_sources(golf PRIVATE ${SIMGOLF_UI_PLAIN} ${SIMGOLF_UI_SDL} src/port/hud_frosted.c src/port/hud_game.c)
# task-31: golfers in the world (tags, thought bubbles, golfer card)
target_sources(golf PRIVATE src/port/golfers_frosted.c src/port/golfers_game.c)
# task-32: the front end (start menu, difficulty, property, load, championship, themes)
target_sources(golf PRIVATE src/port/frontend_frosted.c src/port/frontend_game.c)
# task-27: dialogs (popup menus, check lists, text entry, ...)
target_sources(golf PRIVATE src/port/dialogs_frosted.c src/port/dialogs_game.c)
# task-24: the Options page (tabs of settings over the mods store; options.ini)
target_sources(golf PRIVATE src/port/options_frosted.c)
# task-28: the reports (one tabbed glass panel)
target_sources(golf PRIVATE src/port/reports_frosted.c src/port/reports_game.c)

add_executable(ui_tests "${CMAKE_SOURCE_DIR}/../tests/ui/test_ui.c" ${SIMGOLF_UI_PLAIN} third_party/tomlc99/toml.c)
add_executable(ui_gallery src/port/ui/ui_gallery.c ${SIMGOLF_UI_PLAIN} ${SIMGOLF_UI_SDL} third_party/tomlc99/toml.c)
foreach(t ui_tests ui_gallery)
  target_include_directories(${t} PRIVATE src third_party third_party/stb src/compat/msvc "${SIMGOLF_SDL2}/include")
  target_compile_definitions(${t} PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS _USE_MATH_DEFINES)
  target_compile_options(${t} PRIVATE $<$<COMPILE_LANGUAGE:C>:/clang:-std=gnu11> -Wno-unused-function)
endforeach()
target_link_libraries(ui_gallery PRIVATE "${SIMGOLF_SDL2}/lib/x86/SDL2.lib")
set_target_properties(ui_gallery PROPERTIES WIN32_EXECUTABLE FALSE)

# The tokens next to the exe (ui/frosted_mint.toml), re-copied when edited.
add_custom_target(ui_token_files ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/ui"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/../data/ui/frosted_mint.toml" "${CMAKE_BINARY_DIR}/ui"
  VERBATIM)
