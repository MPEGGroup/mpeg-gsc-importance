#!/bin/bash

CURDIR=$( cd "$( dirname "$0" )" && pwd ); 
echo -e "\033[0;32mBuild: ${CURDIR} \033[0m";

CMAKE=$( command -v cmake )
if [[ -z "${CMAKE}" ]]; then
  echo "Can't find cmake >= 3.17"
  exit 1
fi

print_usage()
{
  echo "$0 importance-scores building script:"
  echo ""
  echo "    Usage:"
  echo "       -h|--help       : Display this information."
  echo "       -o|--output     : Output build directory."
  echo "       --debug         : Build in debug mode."
  echo "       --release       : Build in release mode."
  echo "       --format        : Format C++ sources with clang-format."
  echo "       --cppcheck      : Run static analysis with cppcheck."
  echo "       --noomp         : Build without OpenMP."
  echo "       -c|--compiler   : Compiler: {msvc, clang, gcc}"
  echo ""
  echo "    Examples:"
  echo "      $0"
  echo "      $0 --debug"
  echo "      $0 --noomp"
  echo "      $0 --compiler=gcc"
  if [[ $# -ne 0 ]]; then echo -e "ERROR: $1\n"; fi
  exit 1
}

MODE=Release
OUTPUT=build
COMPILER=""
TARGET=""
CMAKE_FLAGS=()
NUMBER_OF_PROCESSORS=1
if [[ "$( uname -s )" == "Linux"* ]]; then
  NUMBER_OF_PROCESSORS=$( grep -c ^processor /proc/cpuinfo )
  if [[ ${NUMBER_OF_PROCESSORS} -gt 8 ]]; then NUMBER_OF_PROCESSORS=8; fi
fi

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help) print_usage ;;
    -o|--output)
      [[ -n "$2" ]] || print_usage "missing output directory"
      OUTPUT=$2
      shift 2
      ;;
    --output=*) OUTPUT=${1#*=}; shift ;;
    --debug) MODE=Debug; shift ;;
    --release) MODE=Release; shift ;;
    --format) TARGET=clang-format; shift ;;
    --cppcheck) TARGET=cppcheck; shift ;;
    --noomp) CMAKE_FLAGS+=( "-DUSE_OPENMP=OFF" ); shift ;;
    -c|--compiler)
      [[ -n "$2" ]] || print_usage "missing compiler"
      COMPILER=$2
      shift 2
      ;;
    --compiler=*) COMPILER=${1#*=}; shift ;;
    *) print_usage "unsupported argument: $1" ;;
  esac
done

case "${COMPILER}" in
  "") ;;
  msvc) OUTPUT=${OUTPUT}/msvc ;;
  clang)
    OUTPUT=${OUTPUT}/clang
    CLANG_COMPILER=clang++
    if [[ "$( basename "${CXX:-}" )" == clang++* ]]; then CLANG_COMPILER=${CXX}; fi
    CMAKE_FLAGS+=( "-G" "Ninja" "-DCMAKE_CXX_COMPILER=${CLANG_COMPILER}" )
    ;;
  gcc)
    OUTPUT=${OUTPUT}/gcc
    GCC_COMPILER=${CXX:-g++}
    CMAKE_FLAGS+=( "-G" "Ninja" "-DCMAKE_CXX_COMPILER=${GCC_COMPILER}" )
    ;;
  *) print_usage "unsupported compiler: ${COMPILER}" ;;
esac

BUILD_DIR=${CURDIR}/${OUTPUT}/${MODE}
CMAKE_FLAGS+=( "-DCMAKE_BUILD_TYPE=${MODE}" "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON" )

if ! "${CMAKE}" -S "${CURDIR}" -B "${BUILD_DIR}" "${CMAKE_FLAGS[@]}"; then
  echo -e "\033[1;31mfailed \033[0m"
  exit 1
fi

BUILD_COMMAND=( "${CMAKE}" --build "${BUILD_DIR}" --config "${MODE}" --parallel "${NUMBER_OF_PROCESSORS}" )
if [[ -n "${TARGET}" ]]; then BUILD_COMMAND+=( --target "${TARGET}" ); fi

if ! "${BUILD_COMMAND[@]}"; then
  echo -e "\033[1;31mfailed \033[0m"
  exit 1
fi

echo -e "\033[0;32mdone \033[0m"