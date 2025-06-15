#!/usr/bin/env bash

## build.sh
# This is the official build script for the SystemUP, for it to work you need to have the following tools installed:
# Git Bash, and GCC (GNU Compiler Collection).
##

## Usage:
# ./build.sh [OPTIONS]
# Options:
#   -h, --help      Show this help message
#   -v, --version   Show version information
#   -c, --clean     Clean the build directory before building
#   -b, --build     Build the project
#   -t, --test      Run tests after building
#   -d, --debug     Enable debug mode
#   -o, --output    Specify output directory (default: build)

__NAME__=$(basename "$0")
__VERSION__="0.0.1"
__AUTHOR__="h4rl"
__DESCRIPTION__="Build script for SystemUP"
__LICENSE__="BSD 3-Clause License"

# This script is free software: you can redistribute it and/or modify
# it under the terms of the BSD 3-Clause License.
# This script is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# BSD 3-Clause License for more details.
# You should have received a copy of the BSD 3-Clause License
# along with this script.  If not, see <https://opensource.org/license/bsd-3-clause/>.
# This script is intended to be run from the root directory of the SystemUP project.
# It will build the project and output the results to the 'build' directory.

COLORS=true # set to false to disable colored output


OUTPUT_DIR="./build"
DEBUG=false
TEST=false

C_FLAGS="-Wall -Wextra -Werror -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-unused-but-set-parameter -Wno-unused-variable -pedantic -std=c99 -O2"
INCLUDE_DIRS="-I./include"
LD_FLAGS="-mwindows -lpowrprof -lversion"

TEST_FILES=""
C_FILES=""

CC="gcc" # doesn't support address sanitizer, so we use clang if debug is enabled

if ${COLORS}; then
  ESCAPE=$(printf "\e")
	RED="${ESCAPE}[0;31m"
	GREEN="${ESCAPE}[0;32m"
	YELLOW="${ESCAPE}[1;33m"
	BLUE="${ESCAPE}[0;34m"
	CYAN="${ESCAPE}[0;36m"
	CLEAR="${ESCAPE}[0m"
fi

print_line() {
  level=$1
  message=$2

  if [ -z "$message" ]; then
    message="No message provided"
  fi

  if [ -z "$level" ]; then
    level="info"
  fi

  case $level in
  "info")
    echo -e "${GREEN}>${CLEAR} $message"
    ;;
  "warn")
    echo -e "${YELLOW}?${CLEAR} $message"
    ;;
  "error")
    echo -e "${RED}! $message${CLEAR}"
    exit 1
    ;;
  "success")
    echo -e "${GREEN}✓${CLEAR} $message"
    ;;
  *)
    echo -e "${GREEN}>${CLEAR} $message"
    ;;
  esac
}


usage() {
cat <<EOF
${BLUE}Usage${CLEAR}: $__NAME__ [OPTIONS] <args>
$__DESCRIPTION__

${GREEN}Author${CLEAR}: $__AUTHOR__
${GREEN}Version${CLEAR}: $__VERSION__
${GREEN}License${CLEAR}: $__LICENSE__

Options:
${RED}  -h --help               ${CLEAR}  Show this help message
${RED}  -v --version            ${CLEAR}  Show version information
${RED}  -c --compile            ${CLEAR}  Compile the project
${RED}  -l --link               ${CLEAR}  Link the object files
${RED}  -b --build (no args)    ${CLEAR}  Build the project
${RED}  -t --test               ${CLEAR}  Run tests after building
${RED}  -d --debug              ${CLEAR}  Enable debug mode (enables verbose output, debugging symbols, and the address sanitizer)
${RED}  -o --output ${CYAN}<dir>${CLEAR}         Specify output directory (default: build)
${RED}  -z --zip                ${CLEAR}  Create a zip file for release
EOF
exit 0
}


config_compile() {
  local c_file_list
  local test_file_list

  local c_file
  local test_file

  if $TEST; then
    test_file_list=$(find ./tests -type f -name "*.c")
  else
    c_file_list=$(find ./src -type f -name "*.c")
  fi

  if [ -z "$c_file_list" ]; then
    print_line "error" "No C files found in ./src"
  fi

  if $TEST && [ -z "$test_file_list" ]; then
    print_line "error" "No C test files found in ./tests"
  fi

  if $TEST; then
    print_line "info" "Preparing tests for compilation..."
    for test_file in $test_file_list; do
      TEST_FILES+="$test_file "
    done
  fi

  print_line "info" "Preparing source files for compilation..."
  for c_file in $c_file_list; do
    C_FILES+="$c_file "
  done

  if ${DEBUG}; then
      print_line "info" "Building with debug enabled"
      CC="clang"
      C_FLAGS+=" -fsanitize=address -g"
      LD_FLAGS+=" -fsanitize=address"
  fi

  print_line "info" "Building resource file..."
  if ! windres -i ./assets/resource.rc -O coff -o "$OUTPUT_DIR/out/systemup_res.o"; then
    print_line "error" "Failed to build resource file"
  fi

  print_line "info" "Making build directory"
  mkdir -p "$OUTPUT_DIR/bin"
  mkdir -p "$OUTPUT_DIR/out"
  mkdir -p "$OUTPUT_DIR/tests/out"
  mkdir -p "$OUTPUT_DIR/tests/bin"
}

compile() {
  local source_file
  local test_file

  local source_index
  local source_total

  local test_index
  local test_total

  config_compile

  source_total=$(echo "$C_FILES" | wc -w)
  source_index=1

  test_total=$(echo "$TEST_FILES" | wc -w)
  test_index=1

  for source_file in $C_FILES; do
    print_line "info" "[${source_index}/${source_total}] - Compiling $source_file..."
    if "$CC" -c $C_FLAGS $INCLUDE_DIRS $source_file -o "$OUTPUT_DIR/out/$(basename "$source_file" .c).o" $LD_FLAGS; then
      print_line "success" "[${source_index}/${source_total}] - Compiled  $source_file"
      source_index=$((source_index + 1))
    else
      print_line "error" "Failed to compile $source_file"
    fi
  done

  if $TEST; then
    for test_file in $TEST_FILES; do
      print_line "info" "[${test_index}/${test_total}] - Compiling test $test_file..."
      if "$CC" -c $C_FLAGS $INCLUDE_DIRS $test_file -o "$OUTPUT_DIR/tests/out/$(basename "$test_file" .c).o" $LD_FLAGS; then
        print_line "success" "[${test_index}/${test_total}] - Compiled test $test_file"
        test_index=$((test_index + 1))
      else
        print_line "error" "Failed to compile test $test_file"
      fi
    done
  fi

  print_line "success" "Done compiling!"
}

link_() {
  O_FILES=$(find "$OUTPUT_DIR/out" -name "*.o")

  if [ -z "$O_FILES" ]; then
    print_line "error" "No object files found in ./build/out"
  fi

  if "$CC" -o "${OUTPUT_DIR}/bin/systemup.exe" $O_FILES $LD_FLAGS; then
    print_line "success" "Successfully linked object files to ./build/bin/systemup.exe"
  else
    print_line "error" "Failed to link object files to systemup.exe"
  fi

  exit 0
}

build() {
  print_line "info" "Building SystemUP..."
  compile
  link_
}

zip_for_release() {
  if [ ! -d "$OUTPUT_DIR" ]; then
    print_line "error" "Output directory $OUTPUT_DIR does not exist. Please build first."
  fi

  if [ -f "$OUTPUT_DIR/systemup.zip" ]; then
    rm "$OUTPUT_DIR/systemup.zip"
  fi

  print_line "info" "Zipping the build directory..."
  if zip -j "$OUTPUT_DIR/systemup.zip" "$OUTPUT_DIR/bin/systemup.exe" ./assets/install.ps1 ./assets/readme.txt; then
    print_line "success" "Successfully created systemup.zip in $OUTPUT_DIR"
  else
    print_line "error" "Failed to create systemup.zip"
  fi

  exit 0
}


if [ $# -eq 0 ]; then
  build
fi

i=0
for arg in "$@"; do
  case $arg in
    -h|--help)
      usage
      ;;
    -v|--version)
      print_line "info" "$__NAME__ version $__VERSION__"
      exit 0
      ;;
    -c|--compile)
      compile
      ;;
    -l|--link)
      link_
      ;;
    -b|--build)
      build
      ;;
    -t|--test)
      TEST=true
      print_line "info" "Will run tests after building"
      ;;
    -d|--debug)
      DEBUG=true
      print_line "info" "Debug mode enabled"
      ;;
    -o|--output)
      OUTPUT_DIR_IDX=$((i + 1))
      OUTPUT_DIR="${*:$OUTPUT_DIR_IDX:1}"
      ;;
    -z|--zip)
      zip_for_release
      ;;
    *)
      usage
      ;;
  esac
  i=$((i + 1))

  if [ $i -eq $# ]; then
    build
  fi
done