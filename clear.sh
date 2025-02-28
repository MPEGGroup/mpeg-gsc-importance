#!/bin/bash

CURDIR=$( cd "$( dirname "$0" )" && pwd ); 
echo -e "\033[0;32mClean: $CURDIR \033[0m"; 

rm -rf ${CURDIR}/build

if [ "$#" -gt "0" ] 
then
  for name in glfw \
              glm 
  do
    if [ "$1" == "${name}" ] || [ "$1" == "all" ]
    then 
      echo -e "\033[0;32mClean: ${CURDIR}/dependencies/${name} \033[0m";
      rm -rf "${CURDIR}/dependencies/${name}";
    fi
  done
fi
