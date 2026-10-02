
/* The copyright in this software is being made available under the BSD
 * Licence, included below.  This software may be subject to other third
 * party and contributor rights, including patent rights, and no such
 * rights are granted under this licence.
 *
 * Copyright (c) 2025, ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the ISO/IEC nor the names of its contributors
 *   may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once

#define NOMINMAX
#define _USE_MATH_DEFINES
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <istream>
#include <limits>
#include <list>
#include <numeric>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <tuple>
#include <utility>
#include <vector>

#ifdef WIN32
#include <windows.h>
#endif

#define GLM_ENABLE_EXPERIMENTAL
#define GLM_FORCE_SWIZZLE
#include <glm/glm.hpp>
#include <glm/ext/matrix_relational.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#ifdef WIN32
#define snprintf _snprintf
#endif
template <typename... Args>
std::string stringFormat( const char *pFormat, Args... args ) {
  size_t      iSize = snprintf( nullptr, 0, pFormat, args... );
  std::string pString;
  pString.reserve( iSize + 1 );
  pString.resize( iSize );
  snprintf( &pString[0], iSize + 1, pFormat, args... );
  return pString;
}

static inline bool exist( const std::string &pString ) {
  struct stat buffer;
  return ( stat( pString.c_str(), &buffer ) == 0 );
}

static bool dirExists( const std::string &pString ) {
  struct stat sb;
  if ( stat( pString.c_str(), &sb ) == 0 ) { return true; }
  return false;
}

#ifndef _WIN32
static char getSeparator() { return '/'; }
#else
static char getSeparator() { return '\\'; }
#endif

static char getSeparator( const std::string &eFilename ) {
  auto pos = (std::min)( eFilename.find_last_of( '/' ), eFilename.find_last_of( '\\' ) );
  return pos != std::string::npos ? eFilename[pos] : getSeparator();
}

static std::string createFilename( const std::string &pFilename, int32_t iFrameIndex ) {
  std::string sFilename = stringFormat( pFilename.c_str(), iFrameIndex );
#ifdef MSVC
  char pNewName[512];
  GetFullPathName( sFilename.c_str(), 512, pNewName, NULL );
  sFilename = pNewName;
#endif
  return sFilename;
}

inline float deterministic_exp(float x) {
    if (x < -87.33654f) return 0.0f;
    if (x >  88.72283f) return 3.4028235e38f;
    const float INV_LN2 = 1.4426950408889634f;
    float k_f = std::floor(x * INV_LN2 + 0.5f);
    int32_t k = static_cast<int32_t>(k_f);
    const float LN2_HI = 0.6931457519f;
    const float LN2_LO = 1.4286067653e-6f;
    float r = (x - k_f * LN2_HI) - k_f * LN2_LO;
    float p = 0.04166666f;
    p = p * r + 0.16666667f;
    p = p * r + 0.5f;
    p = p * r + 1.0f;
    p = p * r + 1.0f;
    p = std::ldexp(p, k);
    return std::max(p, 0.0f);
}

typedef float ( *CastFunction )( uint8_t *pPointer );

static inline float castUChar( uint8_t *pPointer ) { return (float)( *( (uint8_t *)pPointer ) ); }
static inline float castChar( uint8_t *pPointer ) { return (float)( *( (char *)pPointer ) ); }
static inline float castUShort( uint8_t *pPointer ) { return (float)( *( (uint16_t *)pPointer ) ); }
static inline float castShort( uint8_t *pPointer ) { return (float)( *( (short *)pPointer ) ); }
static inline float castUInt( uint8_t *pPointer ) { return (float)( *( (uint32_t *)pPointer ) ); }
static inline float castInt( uint8_t *pPointer ) { return (float)( *( (int32_t *)pPointer ) ); }
static inline float castFloat( uint8_t *pPointer ) { return (float)( *( (float *)pPointer ) ); }
static inline float castDouble( uint8_t *pPointer ) { return (float)( *( (double *)pPointer ) ); }
static inline float castZero( uint8_t * ) { return 0; }

static void getSize( const std::string &pFormat, int32_t &iSize, CastFunction &eCastFunction ) {
  // clang-format off
  if      ( pFormat == "float"   ) { iSize = 4; eCastFunction = castFloat;   } 
  else if ( pFormat == "uchar"   ) { iSize = 1; eCastFunction = castUChar;   } 
  else if ( pFormat == "uint8"   ) { iSize = 1; eCastFunction = castUChar;   }   
  else if ( pFormat == "char"    ) { iSize = 1; eCastFunction = castChar;    } 
  else if ( pFormat == "int8"    ) { iSize = 1; eCastFunction = castChar;    } 
  else if ( pFormat == "ushort"  ) { iSize = 2; eCastFunction = castUShort;  } 
  else if ( pFormat == "uint16"  ) { iSize = 2; eCastFunction = castUShort;  } 
  else if ( pFormat == "short"   ) { iSize = 2; eCastFunction = castShort;   } 
  else if ( pFormat == "int16"   ) { iSize = 2; eCastFunction = castShort;   } 
  else if ( pFormat == "uint"    ) { iSize = 4; eCastFunction = castUInt;    }
  else if ( pFormat == "uint32"  ) { iSize = 4; eCastFunction = castUInt;    } 
  else if ( pFormat == "int"     ) { iSize = 4; eCastFunction = castInt;     } 
  else if ( pFormat == "int32"   ) { iSize = 4; eCastFunction = castInt;     }
  else if ( pFormat == "float32" ) { iSize = 4; eCastFunction = castFloat;   }
  else if ( pFormat == "float64" ) { iSize = 8; eCastFunction = castDouble;  }
  else if ( pFormat == "double"  ) { iSize = 8; eCastFunction = castDouble;  } 
  else {    iSize         = 0;    eCastFunction = castZero;  }
  // clang-format on
}