# Replays ptt-normal.json and diffs the log against the checked-in expectation.
execute_process(
  COMMAND "${REPLAY_BIN}" --scenario tests/scenarios/ptt-normal.json
          --log "${BIN_DIR}/ptt-normal.actual.log"
  WORKING_DIRECTORY "${SRC_DIR}"
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "passport-replay failed with exit ${rc}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" -E compare_files
          "${BIN_DIR}/ptt-normal.actual.log"
          "${SRC_DIR}/tests/scenarios/ptt-normal.expected.log"
  RESULT_VARIABLE diff_rc)
if(NOT diff_rc EQUAL 0)
  message(FATAL_ERROR "replay log differs from tests/scenarios/ptt-normal.expected.log")
endif()
