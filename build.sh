#!/bin/bash

CURDIR=$( cd "$( dirname "$0" )" && pwd ); 
echo -e "\033[0;32mBuild: ${CURDIR} \033[0m";

CMAKE=""; 
if [ "$( cmake  --version 2>&1 | grep version | awk '{print $3 }' | awk -F '.' '{print $1}' )" == 3 ] ; then CMAKE=cmake; fi
if [ "$( cmake3 --version 2>&1 | grep version | awk '{print $3 }' | awk -F '.' '{print $1}' )" == 3 ] ; then CMAKE=cmake3; fi
if [ "$CMAKE" == "" ] ; then echo "Can't find cmake > 3.0"; exit; fi

print_usage()
{
  echo "$0 mpeg-vmesh-tm building script: "
  echo "";
  echo "    Usage:" 
  echo "       -h|--help     : Display this information."  
  echo "       -o|--output   : Output build directory."
  echo "       -n|--ninja    : Use Ninja."
  echo "       --debug       : Build in debug mode."
  echo "       --release     : Build in release mode."
  echo "       --format      : Format source code."
  echo "       --tidy        : Check source code with clang-tidy."
  echo "       --cppcheck    : Check source code with cppcheck."
  echo "       --doc         : Build documentation (latex and pdflatex requiered)"
  echo "       --format      : Format source code"
  echo "       --tidy        : Check source code with clang-tidy"
  echo "       --cppcheck    : Check source code with cppcheck"
  echo "       --noomp       : build without openMP"
  echo "       --cpu_only    : build cpu only version"
  echo "       -c|--compiler : Compiler: {msvc, clang, gcc}" 
  echo "";
  echo "    Examples:";
  echo "      $0 "; 
  echo "      $0 --debug"; 
  echo "      $0 --doc";   
  echo "      $0 --format";    
  echo "    ";  
  if [ $# != 0 ] ; then echo -e "ERROR: $1 \n"; fi
  exit 1;
}

MODE=Release
TARGETS=()
CMAKE_FLAGS=()
OUTPUT=build
case $(uname -s) in Linux*) NUMBER_OF_PROCESSORS=$( grep -c ^processor /proc/cpuinfo );; esac

while [[ $# -gt 0 ]] ; do  
  C=$1; if [[ "$C" =~ [=] ]] ; then V=${C#*=}; elif [[ $2 == -* ]] ; then  V=""; else V=$2; shift; fi;
  case "$C" in    
    -h|--help      ) print_usage ;;
    -o|--output=*  ) OUTPUT=${V};;
    --debug        ) MODE=Debug; CMAKE_FLAGS+=("-DCMAKE_C_FLAGS=\"-g3\"" "-DCMAKE_CXX_FLAGS=\"-g3\"" );;
    --release      ) MODE=Release;;
    --doc          ) make -C "${CURDIR}/doc/"; exit 0;;
    --format       ) TARGETS+=( "clang-format" );;
    --tidy         ) TARGETS+=( "clang-tidy" );;
    --cppcheck     ) TARGETS+=( "cppcheck" );;       
    --noomp        ) CMAKE_FLAGS+=( "-DUSE_OPENMP=OFF" );;
    --cpu_only     ) CMAKE_FLAGS+=( "-DUSE_CPU_ONLY=ON" );;
    -c|--compiler=*) COMPILER=${V};;
    *              ) print_usage "unsupported arguments: $C ";;
  esac
  shift;
done

CMAKE_FLAGS+=( "-DCMAKE_BUILD_TYPE=$MODE" ) 
CMAKE_FLAGS+=( "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON" )
# CMAKE_FLAGS+=( "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${CURDIR}/${OUTPUT}/${MODE}/bin" )
# CMAKE_FLAGS+=( "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY=${CURDIR}/${OUTPUT}/${MODE}/lib" )
# CMAKE_FLAGS+=( "-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=${CURDIR}/${OUTPUT}/${MODE}/lib" )
# CMAKE_FLAGS+=( "-DCMAKE_CXX_FLAGS=-stdlib=libc++" )

case $COMPILER in
  "")    ;;
  msvc)  OUTPUT=${OUTPUT}/msvc;;
  clang) OUTPUT=${OUTPUT}/clang; CMAKE_FLAGS+=( "-G Ninja" "-DCMAKE_CXX_COMPILER=clang++" "-DCMAKE_C_COMPILER=clang" "-DCMAKE_CXX_FLAGS=\"-stdlib=libc++\"");;
  gcc)   OUTPUT=${OUTPUT}/gcc;   CMAKE_FLAGS+=( "-G Ninja" "-DCMAKE_CXX_COMPILER=g++"     "-DCMAKE_C_COMPILER=gcc");;
  *) echo "Compiler not supported: $COMPILER"; exit;;
esac

echo "${CMAKE_FLAGS[@]}"
if ! ${CMAKE} -H${CURDIR} -B"${CURDIR}/${OUTPUT}/${MODE}" "${CMAKE_FLAGS[@]}";
then
  echo -e "\033[1;31mfailed \033[0m"
  exit 1;
fi 
echo -e "\033[0;32mdone \033[0m";

# Use custom targets
if (( ${#TARGETS[@]} ))
then 
  for TARGET in ${TARGETS[@]}
  do     
    if [ "$TARGET" == "clang-tidy" ]
    then 
      if [ ! -f ${CC}-tidy ]
      then 
        echo "${CC}-tidy not exists"
      else
        ${CC}-tidy \
          -format-style=file \
          -p ${CURDIR}/${OUTPUT}/${MODE} \
          -fix \
          -fix-errors \
          $( find ${CURDIR}/source/ -name '*.cpp' -o -name '*.hpp' )
      fi
    else 
      echo -e "\033[0;32m${TARGET}: ${CURDIR} \033[0m";
      ${CMAKE} --build "${CURDIR}/${OUTPUT}/${MODE}" --target ${TARGET}
      echo -e "\033[0;32mdone \033[0m";
    fi
  done
  exit 0
fi

echo -e "\033[0;32mBuild: ${CURDIR} \033[0m";
echo -e "${CMAKE} --build "${CURDIR}/${OUTPUT}/${MODE}" --config ${MODE} --parallel "${NUMBER_OF_PROCESSORS}"";
if ! ${CMAKE} --build "${CURDIR}/${OUTPUT}/${MODE}" --config ${MODE} --parallel "${NUMBER_OF_PROCESSORS}" ;
then
  echo -e "\033[1;31mfailed \033[0m"
  exit 1;
fi 
echo -e "\033[0;32mdone \033[0m";

