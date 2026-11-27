# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0


@SP-01021
Feature: Semantic version
  PluginGlobal::SemVer parses a version as Semantic Versioning 2.0.0 defines it, and is
  what the plugin manages its own version and the latest released version with, by
  SP-00010. It needs neither libobs nor Qt.

  Background:
    Given "the version is cleared" means that the major, minor and patch versions are 0
    And that the pre-release and the build metadata are empty

  @SP-01021.1
  Scenario Outline: A version core is parsed
    When "<version>" is parsed
    Then parsing succeeds
    And the major, minor and patch versions are <major>, <minor> and <patch>
    And the pre-release and the build metadata are empty

    Examples:
      | version  | major | minor | patch |
      | 1.23.456 | 1     | 23    | 456   |
      | 0.0.0    | 0     | 0     | 0     |

  @SP-01021.2
  Scenario Outline: A pre-release and build metadata are parsed
    When "<version>" is parsed
    Then parsing succeeds
    And the pre-release is "<pre-release>" and the build metadata is "<build>"

    Examples:
      | version                   | pre-release | build       |
      | 10.20.30-rc.1+macos.arm64 | rc.1        | macos.arm64 |
      | 1.0.0-0.3.7               | 0.3.7       |             |
      | 1.0.0+001                 |             | 001         |
      | 1.0.0-x-y.0a+-            | x-y.0a      | -           |
      | 1.0.0-0                   | 0           |             |
      | 1.0.0--                   | -           |             |
      | 1.0.0-1a.b+0.-            | 1a.b        | 0.-         |

  @SP-01021.3
  Scenario Outline: A version number has at most 18 digits
    When "<accepted>" is parsed
    Then parsing succeeds with the <component> version 999999999999999999
    And parsing "<rejected>" fails and the version is cleared

    Examples:
      | component | accepted               | rejected                |
      | major     | 999999999999999999.0.0 | 1000000000000000000.0.0 |
      | minor     | 0.999999999999999999.0 | 0.1000000000000000000.0 |
      | patch     | 0.0.999999999999999999 | 0.0.1000000000000000000 |

  @SP-01021.4
  Scenario Outline: A pre-release and build metadata have at most 100 characters
    Given <text>, 100 characters in all
    When "1.0.0-" followed by the text, "+" and the text again is parsed
    Then parsing succeeds with the text as the pre-release and as the build metadata
    And parsing fails and the version is cleared when either has one more character "x"

    Examples:
      | text                                                 |
      | one identifier of letters "a"                        |
      | identifiers "x" separated by dots, the dots included |

  @SP-01021.5
  Scenario Outline: An invalid version is rejected
    Given "9.9.9-pre+build" has been parsed
    When "<version>" is parsed
    Then parsing fails and the version is cleared

    Examples:
      | version    |
      |            |
      | 1          |
      | 1.2        |
      | 1.2.3.4    |
      | 1..2.3     |
      | 01.2.3     |
      | 1.02.3     |
      | 1.2.03     |
      | v1.2.3     |
      | -1.2.3     |
      | " 1.2.3"   |
      | "1.2.3 "   |
      | 1.2.3-     |
      | 1.2.3+     |
      | 1.2.3-+a   |
      | 1.2.3-01   |
      | 1.2.3-a.01 |
      | 1.2.3-a.   |
      | 1.2.3-a..b |
      | 1.2.3-a_b  |
      | 1.2.3-é    |
      | 1.2.3+.a   |
      | 1.2.3+a..b |
      | 1.2.3+a+b  |

  @SP-01021.6
  Scenario Outline: A NUL character inside the input is rejected
    When "<version>", whose length counts the NUL and the bytes after it, is parsed
    Then parsing fails and the version is cleared

    Examples:
      | version       |
      | 1.2.3\0       |
      | 1.2.3\0junk   |
      | 1.2.3-a\0junk |
      | 1.2.3+a\0junk |

  @SP-01021.7
  Scenario: A copy keeps its own version
    Given "1.2.3-rc.1+b.7" has been parsed
    When the version is copied, and the original parses another version and is destroyed
    Then the copy still has the version 1.2.3, the pre-release "rc.1" and the build metadata "b.7"

  @SP-01021.8
  Scenario: Versions are ordered by precedence
    Given these versions, in ascending order of precedence:
      | version          | why it follows the one before                                       |
      | 1.0.0-1          |                                                                     |
      | 1.0.0-2          | numeric identifiers are compared numerically                        |
      | 1.0.0-10         | numeric identifiers are compared numerically, not as text           |
      | 1.0.0--          | a numeric identifier is less than an alphanumeric one               |
      | 1.0.0-1a         | alphanumeric identifiers are compared in ASCII order                |
      | 1.0.0-Z          | alphanumeric identifiers are compared in ASCII order                |
      | 1.0.0-alpha      | alphanumeric identifiers are compared in ASCII order                |
      | 1.0.0-alpha.1    | a longer list of identifiers with an equal prefix is greater        |
      | 1.0.0-alpha.beta | a numeric identifier is less than an alphanumeric one               |
      | 1.0.0-alphabet   | "alpha" is less than "alphabet"                                     |
      | 1.0.0-beta       | alphanumeric identifiers are compared in ASCII order                |
      | 1.0.0-beta.2     | a longer list of identifiers with an equal prefix is greater        |
      | 1.0.0-beta.11    | numeric identifiers are compared numerically, not as text           |
      | 1.0.0-rc.1       | alphanumeric identifiers are compared in ASCII order                |
      | 1.0.0            | a version without a pre-release is greater than one with            |
      | 1.0.1-0          | the version core is compared before the pre-release                 |
      | 1.0.1            | a version without a pre-release is greater than one with            |
      | 1.1.0            | the minor version is compared before the patch version              |
      | 2.0.0            | the major version is compared before the minor version              |
      | 10.0.0           | version numbers are compared numerically, not as text               |
    When every two of them are compared
    Then the one listed earlier is less than the one listed later
    And a version is equal only to itself

  @SP-01021.9
  Scenario Outline: Build metadata does not affect precedence
    When "<left>" and "<right>" are compared
    Then they are equal

    Examples:
      | left         | right        |
      | 1.0.0+a      | 1.0.0+b      |
      | 1.0.0        | 1.0.0+build  |
      | 1.0.0-rc.1+a | 1.0.0-rc.1   |
      | 1.0.0-rc.1+a | 1.0.0-rc.1+b |

  @SP-01021.10
  Scenario: A version that has not been parsed is cleared
    When a version is created and nothing is parsed into it
    Then the version is cleared
    And it is equal to "0.0.0"
