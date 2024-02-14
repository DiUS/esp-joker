idf_component_register(
  SRCS
    "main.c"
    "esp-box.c"
  INCLUDE_DIRS include
  REQUIRES
    "driver"
    "esp-sr"
    "picotts"
  PRIV_INCLUDE_DIRS "${CMAKE_CURRENT_BINARY_DIR}"
)

add_custom_command(
  OUTPUT "jokes.ic"
  COMMAND "${COMPONENT_DIR}/scripts/c-text-array.sh" "${COMPONENT_DIR}/dadjokes/controllers/jokes.txt" "jokes" "jokes.ic"
  DEPENDS "${COMPONENT_DIR}/dadjokes/controllers/jokes.txt"
  VERBATIM
)
add_custom_target(jokes DEPENDS "jokes.ic")
add_dependencies(${COMPONENT_LIB} jokes)
set_property(
  DIRECTORY "${COMPONENT_DIR}"
  APPEND PROPERTY ADDITIONAL_CLEAN_FILES "jokes.ic"
)