#!/bin/bash

CURDIR=$( cd "$( dirname "$0" )" && pwd )
echo -e "\033[0;32mClean: ${CURDIR} \033[0m"

if [[ $# -eq 0 ]]; then
  rm -rf "${CURDIR}/build"
  exit 0
fi

case "$1" in
  msvc|clang|gcc)
    echo -e "\033[0;32mClean: ${CURDIR}/build/$1 \033[0m"
    rm -rf "${CURDIR}/build/$1"
    ;;
  glm)
    echo -e "\033[0;32mClean: ${CURDIR}/dependencies/glm \033[0m"
    rm -rf "${CURDIR}/dependencies/glm"
    ;;
  all)
    rm -rf "${CURDIR}/build" "${CURDIR}/dependencies/glm"
    ;;
  *)
    echo "Usage: $0 [all|msvc|clang|gcc|glm]"
    exit 1
    ;;
esac