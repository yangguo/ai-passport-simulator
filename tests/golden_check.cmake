# Replays one fixture, renders a checkpoint screenshot, and diffs it against the
# checked-in golden. Required -D flags: REPLAY_BIN, COMPARE_BIN, BIN_DIR,
# SRC_DIR, FIXTURE, GOLDEN. Optional: AT_MS (-1 = final state), ALERT (""
# = no overlay).
if(AT_MS GREATER_EQUAL 0)
  set(at_arg --at-ms ${AT_MS})
endif()
if(DEFINED ALERT AND NOT ALERT STREQUAL "")
  set(alert_arg --alert ${ALERT})
endif()
execute_process(
  COMMAND "${REPLAY_BIN}" --scenario tests/scenarios/${FIXTURE}.json
          --screenshot "${BIN_DIR}/${GOLDEN}.actual.png" ${at_arg} ${alert_arg}
  WORKING_DIRECTORY "${SRC_DIR}"
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "passport-replay failed with exit ${rc}")
endif()
execute_process(
  COMMAND "${COMPARE_BIN}" "${SRC_DIR}/tests/golden/${GOLDEN}.png"
          "${BIN_DIR}/${GOLDEN}.actual.png" "${BIN_DIR}/${GOLDEN}.diff.png"
  RESULT_VARIABLE diff_rc
  OUTPUT_VARIABLE diff_out)
message(STATUS "${diff_out}")
if(NOT diff_rc EQUAL 0)
  message(FATAL_ERROR "golden ${GOLDEN} differs; see ${GOLDEN}.diff.png")
endif()
