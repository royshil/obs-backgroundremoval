# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

function(discover_test_cases SOURCE_FILE OUTPUT_VARIABLE)
  file(STRINGS "${SOURCE_FILE}" TEST_FUNCTIONS
       REGEX "^[ \t]*int test[A-Za-z0-9_]+\\(\\)[ \t]*$")
  set(TEST_CASES)
  foreach(TEST_FUNCTION IN LISTS TEST_FUNCTIONS)
    string(REGEX REPLACE "^[ \t]*int (test[A-Za-z0-9_]+)\\(\\)[ \t]*$" "\\1"
           TEST_CASE "${TEST_FUNCTION}")
    list(APPEND TEST_CASES "${TEST_CASE}")
  endforeach()
  list(SORT TEST_CASES)
  set("${OUTPUT_VARIABLE}" "${TEST_CASES}" PARENT_SCOPE)
endfunction()

add_subdirectory(UI)
add_subdirectory(UpdateConfig)
