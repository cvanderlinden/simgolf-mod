# Round HD golf ball (card task-41): game side (hdball.c), render side
# (hdball_render.c) and its tuning file next to the exe (ui/ball.txt).
# Included by native.cmake after the golf target. The hooks are in
# src/port/golf/ball_hooks.c (picked up with the other overrides).
target_sources(golf PRIVATE src/port/hdball.c src/port/hdball_render.c)
add_custom_target(ball_files ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/ui"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/../data/ui/ball.txt" "${CMAKE_BINARY_DIR}/ui"
  VERBATIM)
