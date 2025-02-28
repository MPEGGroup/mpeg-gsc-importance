
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

#include "common.hpp"

struct GaussianData {
  glm::vec3 xyz_;
  float     opacity_;
  glm::vec4 sh_[3][4];
  glm::vec3 scale_;
  glm::quat rotate_;
  glm::vec3 normal_;

  inline glm::mat3 cov() const {
    glm::mat3 R( glm::normalize( rotate_ ) );
    glm::mat3 S( glm::vec3( expf( scale_[0] ), 0.0f, 0.0f ),  //
                 glm::vec3( 0.0f, expf( scale_[1] ), 0.0f ),  //
                 glm::vec3( 0.0f, 0.0f, expf( scale_[2] ) ) );
    return R * S * glm::transpose( S ) * glm::transpose( R );
  }

  inline glm::vec3 computeRadiance( const glm::vec3& direction ) const {
    const float k[10]  = { 0.28209479177387814f, 0.4886025119029199f, 1.0925484305920792f, 0.31539156525252005f,
                          0.5462742152960396f,  0.5900435899266435f, 2.8906114426405543f, 0.4570457994644658f,
                          0.37317633259011546f, 1.4453057213202771f };
    glm::vec3   v      = glm::normalize( direction );
    glm::vec3   v2     = v * v;
    glm::vec4   b0     = glm::vec4( k[0], -k[1] * v.y, k[1] * v.z, -k[1] * v.x );
    glm::vec4   b1     = glm::vec4( k[2] * v.y * v.x, -k[2] * v.y * v.z, k[3] * ( 3 * v2.z - 1 ), -k[2] * v.x * v.z );
    glm::vec4   b2     = glm::vec4( k[4] * ( v2.x - v2.y ), -k[5] * v.y * ( 3 * v2.x - v2.y ), k[6] * v.y * v.x * v.z,
                              -k[7] * v.y * ( 5 * v2.z - 1 ) );
    glm::vec4   b3     = glm::vec4( k[8] * v.z * ( 5 * v2.z - 3 ), -k[7] * v.x * ( 5 * v2.z - 1 ),
                              k[9] * v.z * ( v2.x - v2.y ), -k[5] * v.x * ( v2.x - 3 * v2.y ) );
    glm::vec3   result = glm::vec3( glm::dot( sh_[0][0], b0 ) + glm::dot( sh_[0][1], b1 ) + glm::dot( sh_[0][2], b2 ) +
                                      glm::dot( sh_[0][3], b3 ) + 0.5,
                                  glm::dot( sh_[1][0], b0 ) + glm::dot( sh_[1][1], b1 ) + glm::dot( sh_[1][2], b2 ) +
                                      glm::dot( sh_[1][3], b3 ) + 0.5,
                                  glm::dot( sh_[2][0], b0 ) + glm::dot( sh_[2][1], b1 ) + glm::dot( sh_[2][2], b2 ) +
                                      glm::dot( sh_[2][3], b3 ) + 0.5 );
    return result;
  }

  inline double alpha() const { return 1.0 / ( 1.0 + exp( (double)( -opacity_ ) ) ); }

  void traceSH() const {
    for ( size_t j = 0; j < 3; j++ ) {
      for ( size_t k = 0; k < 4; k++ ) {
        printf( "SH[%zu][%zu] = %12.6f %12.6f %12.6f %12.6f ", j, k, sh_[j][k][0], sh_[j][k][1], sh_[j][k][2],
                sh_[j][k][3] );
      }
      printf( "\n" );
    }
  }
};

class Pointcloud {
 public:
  Pointcloud() {}
  Pointcloud( const std::string& path, int32_t frameIndex ) { read( path, frameIndex ); }
  ~Pointcloud() { reset(); }

  auto                begin() { return gaussianData_.begin(); }
  auto                end() { return gaussianData_.end(); }
  auto                begin() const { return gaussianData_.cbegin(); }
  auto                end() const { return gaussianData_.cend(); }
  GaussianData&       operator[]( int32_t index ) { return gaussianData_[index]; }
  const GaussianData& operator[]( int32_t index ) const { return gaussianData_[index]; }
  inline int32_t      size() const { return (int32_t)gaussianData_.size(); }
  inline void*        data() const { return (void*)gaussianData_.data(); }
  inline void         reset() { gaussianData_.clear(); }

  bool read( const std::string& path, int32_t frameIndex ) {
    auto filename = createFilename( path, frameIndex );
    if ( !exist( filename ) ) {
      printf( "Error: The input file does not exist: \"%s\"\n", filename.c_str() );
      return false;
    }
    std::string   line, s1, s2;
    std::ifstream inputPly;
    inputPly.open( filename.c_str(), std::ios::in | std::ios::binary );
    if ( !inputPly.is_open() ) {
      printf( "Pointcloud reader: Couldn't open %s \n", filename.c_str() );
      return false;
    }
    bool bFormatError = false, bAscii = false, bLittleEndian = false, bInVertexElements = false,
         bInHeaderElements = false;
    int iNumPoints = 0, iSize = 0, iOrder = 0, iIndex = 0, iNumFaces = 0, iNumHeader = 0, iSizeHeader = 0;
    const std::vector<std::string> pProperty = {
        "x",         "y",         "z",         "nx",        "ny",        "nz",        "f_dc_0",    "f_dc_1",
        "f_dc_2",    "f_rest_0",  "f_rest_1",  "f_rest_2",  "f_rest_3",  "f_rest_4",  "f_rest_5",  "f_rest_6",
        "f_rest_7",  "f_rest_8",  "f_rest_9",  "f_rest_10", "f_rest_11", "f_rest_12", "f_rest_13", "f_rest_14",
        "f_rest_15", "f_rest_16", "f_rest_17", "f_rest_18", "f_rest_19", "f_rest_20", "f_rest_21", "f_rest_22",
        "f_rest_23", "f_rest_24", "f_rest_25", "f_rest_26", "f_rest_27", "f_rest_28", "f_rest_29", "f_rest_30",
        "f_rest_31", "f_rest_32", "f_rest_33", "f_rest_34", "f_rest_35", "f_rest_36", "f_rest_37", "f_rest_38",
        "f_rest_39", "f_rest_40", "f_rest_41", "f_rest_42", "f_rest_43", "f_rest_44", "opacity",   "scale_0",
        "scale_1",   "scale_2",   "rot_0",     "rot_1",     "rot_2",     "rot_3",
    };
    std::vector<int>          pSize( pProperty.size() + 1, 0 );
    std::vector<int>          pOrder( pProperty.size(), -1 );
    std::vector<int>          pIndex( pProperty.size(), 0 );
    std::vector<CastFunction> pCast( pProperty.size() + 1, castZero );
    getline( inputPly, line );
    while ( line.find( "end_header" ) != 0 ) {
      line = line.erase( line.size() );
      line.erase( std::remove( line.begin(), line.end(), '\n' ), line.end() );
      line.erase( std::remove( line.begin(), line.end(), '\r' ), line.end() );
      if ( static_cast<int>( line.find( "format " ) ) > -1 ) {
        std::string format = line.substr(
            line.find( ' ' ) + 1, line.find_first_of( ' ', line.find_first_of( ' ' ) + 1 ) - line.find( ' ' ) - 1 );
        if ( format == "ascii" ) {
          bAscii = true;
        } else if ( format == "binary_little_endian" ) {
          bLittleEndian = true;
        } else {
          printf(
              "GaussianSpatReader: Format is not ascii or "
              "binary_little_endian: format = \"%s\" \n",
              format.c_str() );
          bFormatError = true;
        }
      } else if ( static_cast<int>( line.find( "element" ) ) == 0 ) {
        if ( static_cast<int>( line.find( "header_data " ) ) > 0 ) {
          line              = line.substr( line.find( "header_data " ) );
          iNumHeader        = atoi( line.substr( line.find( ' ' ) ).c_str() );
          bInHeaderElements = true;
          bInVertexElements = false;
        } else if ( static_cast<int>( line.find( "vertex " ) ) > 0 ) {
          line              = line.substr( line.find( "vertex " ) );
          iNumPoints        = atoi( line.substr( line.find( ' ' ) ).c_str() );
          bInHeaderElements = false;
          bInVertexElements = true;
        } else if ( static_cast<int>( line.find( "face " ) ) > 0 ) {
          line              = line.substr( line.find( "face " ) );
          iNumFaces         = atoi( line.substr( line.find( ' ' ) ).c_str() );
          bInHeaderElements = false;
          bInVertexElements = false;
        } else {
          bInHeaderElements = false;
          bInVertexElements = false;
        }
      } else {
        if ( bInVertexElements && static_cast<int>( line.find( "property " ) ) == 0 ) {
          line           = line.substr( line.find( ' ' ) + 1, line.size() - line.find( ' ' ) - 1 );
          std::string eV = line.substr( 0, line.find_last_of( ' ' ) ), eT = line.substr( line.find_last_of( ' ' ) + 1 );
          CastFunction eCast;
          getSize( eV, iSize, eCast );
          bool find = false;
          for ( size_t i = 0; i < pProperty.size() && !find; i++ ) {
            if ( eT == pProperty[i] ) {
              pSize[i]  = iSize;
              pOrder[i] = iOrder;
              pIndex[i] = iIndex;
              pCast[i]  = eCast;
              find      = true;
            }
          }
          if ( !find ) {
            pSize[pProperty.size()] += iSize;
            pCast[pProperty.size()] = eCast;
            printf( "GaussianSplatReader: Property \"%s\" is not supported ( %s ) \n", eT.c_str(), eV.c_str() );
            fflush( stdout );
          }
          iIndex += iSize;
          iOrder++;
        } else if ( bInHeaderElements && static_cast<int>( line.find( "property " ) ) == 0 ) {
          line           = line.substr( line.find( ' ' ) + 1, line.size() - line.find( ' ' ) - 1 );
          std::string eV = line.substr( 0, line.find_last_of( ' ' ) ), eT = line.substr( line.find_last_of( ' ' ) + 1 );
          CastFunction eCast;
          getSize( eV, iSize, eCast );
          iSizeHeader += iSize;
        }
      }
      getline( inputPly, line );
    }
    if ( pSize[0] == 0 ) {
      printf( "GaussianSplatReader: X must be define \n" );
      bFormatError = true;
    }
    if ( pSize[1] == 0 ) {
      printf( "GaussianSplatReader: Y must be define \n" );
      bFormatError = true;
    }
    if ( pSize[2] == 0 ) {
      printf( "GaussianSplatReader: Z must be define \n" );
      bFormatError = true;
    }
    if ( iNumFaces > 0 ) {
      printf(
          "GaussianSplatReader: number of face must be equal to 0 (iNumFaces "
          "= %d) \n",
          iNumFaces );
      bFormatError = true;
    }
    if ( bFormatError ) {
      printf( "GaussianSplatReader: format not yet supported \n" );
      fflush( stdout );
      reset();
      return false;
    }
    gaussianData_.resize( iNumPoints );
    if ( bAscii ) {
      printf( "Gaussian splat ascii is not yet supported \n" );
      fflush( stdout );
      exit( -1 );
    } else if ( bLittleEndian ) {
      std::vector<unsigned char> pC;
      pC.resize( iIndex );
      std::array<float, 48> pSh;
      if ( iNumHeader > 0 ) { inputPly.ignore( iNumHeader * iSizeHeader ); }
      for ( size_t i = 0; i < static_cast<size_t>( iNumPoints ); i++ ) {
        auto& dst = gaussianData_[i];
        inputPly.read( reinterpret_cast<char*>( pC.data() ), iIndex * sizeof( unsigned char ) );

        int k       = 0;
        dst.xyz_[0] = pCast[k + 0]( pC.data() + pIndex[k + 0] );  // x
        dst.xyz_[1] = pCast[k + 1]( pC.data() + pIndex[k + 1] );  // y
        dst.xyz_[2] = pCast[k + 2]( pC.data() + pIndex[k + 2] );  // z
        k += 3;

        dst.normal_[0] = pCast[k + 0]( pC.data() + pIndex[k + 0] );  // nx
        dst.normal_[1] = pCast[k + 1]( pC.data() + pIndex[k + 1] );  // ny
        dst.normal_[2] = pCast[k + 2]( pC.data() + pIndex[k + 2] );  // nz
        k += 3;
        for ( int j = 0; j < 48; j++, k++ ) {
          float sh = pCast[k]( pC.data() + pIndex[k] );  // sh
          pSh[j]   = sh;
        }

        dst.opacity_ = pCast[k]( pC.data() + pIndex[k] );  // o
        k++;

        dst.scale_ = glm::vec3( pCast[k + 0]( pC.data() + pIndex[k + 0] ),    // scale 0
                                pCast[k + 1]( pC.data() + pIndex[k + 1] ),    // scale 1
                                pCast[k + 2]( pC.data() + pIndex[k + 2] ) );  // scale 2
        k += 3;
        dst.rotate_ = glm::quat( pCast[k + 0]( pC.data() + pIndex[k + 0] ),    // rx
                                 pCast[k + 1]( pC.data() + pIndex[k + 1] ),    // ry
                                 pCast[k + 2]( pC.data() + pIndex[k + 2] ),    // rz
                                 pCast[k + 3]( pC.data() + pIndex[k + 3] ) );  // rw
        dst.rotate_ = dst.rotate_;
        k += 4;
        for ( int j = 0, k = 3; j < 3; j++, k += 15 ) {
          dst.sh_[j][0] = glm::vec4( pSh[j], pSh[k], pSh[k + 1], pSh[k + 2] );
          dst.sh_[j][1] = glm::vec4( pSh[k + 3], pSh[k + 4], pSh[k + 5], pSh[k + 6] );
          dst.sh_[j][2] = glm::vec4( pSh[k + 7], pSh[k + 8], pSh[k + 9], pSh[k + 10] );
          dst.sh_[j][3] = glm::vec4( pSh[k + 11], pSh[k + 12], pSh[k + 13], pSh[k + 14] );
        }
      }
      pC.clear();
    }
    inputPly.close();
    return true;
  }

  bool write( const std::string& path, int32_t frameIndex ) {
    auto          filename = createFilename( path, frameIndex );
    std::ofstream file( filename, std::ios::out | std::ios::binary );
    if ( !file.is_open() ) { throw std::runtime_error( "Error: can't open: " + filename ); }
    file << "ply\n";
    file << "format binary_little_endian 1.0\n";
    file << "element vertex " << gaussianData_.size() << "\n";
    file << "property float x\n";
    file << "property float y\n";
    file << "property float z\n";
    file << "property float nx\n";
    file << "property float ny\n";
    file << "property float nz\n";
    for ( int i = 0; i < 3; ++i ) file << "property float f_dc_" << i << "\n";
    for ( int i = 0; i < 45; ++i ) file << "property float f_rest_" << i << "\n";
    file << "property float opacity\n";
    file << "property float scale_0\n";
    file << "property float scale_1\n";
    file << "property float scale_2\n";
    file << "property float rot_0\n";
    file << "property float rot_1\n";
    file << "property float rot_2\n";
    file << "property float rot_3\n";
    file << "end_header\n";

    std::array<float, 48> sh;
    std::array<float, 3>  normal;
    for ( const auto& data : gaussianData_ ) {
      file.write( reinterpret_cast<const char*>( &data.xyz_ ), sizeof( data.xyz_ ) );
      file.write( reinterpret_cast<const char*>( normal.data() ), sizeof( normal ) );
      for ( int j = 0, k = 3; j < 3; j++, k += 15 ) {
        sh[j]      = data.sh_[j][0].x;
        sh[k + 0]  = data.sh_[j][0].y;
        sh[k + 1]  = data.sh_[j][0].z;
        sh[k + 2]  = data.sh_[j][0].w;
        sh[k + 3]  = data.sh_[j][1].x;
        sh[k + 4]  = data.sh_[j][1].y;
        sh[k + 5]  = data.sh_[j][1].z;
        sh[k + 6]  = data.sh_[j][1].w;
        sh[k + 7]  = data.sh_[j][2].x;
        sh[k + 8]  = data.sh_[j][2].y;
        sh[k + 9]  = data.sh_[j][2].z;
        sh[k + 10] = data.sh_[j][2].w;
        sh[k + 11] = data.sh_[j][3].x;
        sh[k + 12] = data.sh_[j][3].y;
        sh[k + 13] = data.sh_[j][3].z;
        sh[k + 14] = data.sh_[j][3].w;
      }
      file.write( reinterpret_cast<const char*>( sh.data() ), sizeof( sh ) );
      file.write( reinterpret_cast<const char*>( &data.opacity_ ), sizeof( data.opacity_ ) );
      file.write( reinterpret_cast<const char*>( &data.scale_ ), sizeof( data.scale_ ) );
      file.write( reinterpret_cast<const char*>( &data.rotate_[3] ), sizeof( data.rotate_[3] ) );
      file.write( reinterpret_cast<const char*>( &data.rotate_[0] ), sizeof( data.rotate_[0] ) );
      file.write( reinterpret_cast<const char*>( &data.rotate_[1] ), sizeof( data.rotate_[1] ) );
      file.write( reinterpret_cast<const char*>( &data.rotate_[2] ), sizeof( data.rotate_[2] ) );
    }
    file.close();
    return true;
  }

  void removeSphericalHarmonics( int32_t number ) {
    if ( number > 16 ) return;
    for ( auto& el : gaussianData_ )
      for ( size_t i = 0; i < 3; i++ )
        for ( size_t j = 0; j < 4; j++ )
          for ( size_t k = 0; k < 4; k++ )
            if ( j * 4 + k >= number ) el.sh_[i][j][k] = 0;
    numSH_ = number;
  }

  std::pair<glm::vec3, glm::vec3> getBoundingBox() {
    glm::vec3 minBound = gaussianData_[0].xyz_;
    glm::vec3 maxBound = gaussianData_[0].xyz_;
    for ( const auto& data : gaussianData_ ) {
      minBound = glm::min( minBound, data.xyz_ );
      maxBound = glm::max( maxBound, data.xyz_ );
    }
    return { minBound, maxBound };
  }

  void quantizePosition( const int32_t bitdepth ) {
    if ( bitdepth >= 32 ) return;
    glm::dvec3 minBound = glm::dvec3( gaussianData_[0].xyz_ );
    glm::dvec3 maxBound = glm::dvec3( gaussianData_[0].xyz_ );
    for ( const auto& data : gaussianData_ ) {
      minBound = glm::min( minBound, glm::dvec3( data.xyz_ ) );
      maxBound = glm::max( maxBound, glm::dvec3( data.xyz_ ) );
    }
    const double     maxLevel = static_cast<double>( ( 1ULL << bitdepth ) - 1 );
    const glm::dvec3 scale    = maxLevel / glm::dvec3( maxBound - minBound );
    for ( auto& data : gaussianData_ ) {
      glm::ivec3 quantized = glm::ivec3( glm::floor( ( glm::dvec3( data.xyz_ ) - minBound ) * scale + 0.5 ) );
      data.xyz_            = glm::vec3( minBound + glm::dvec3( quantized ) / scale );
    }
    bitdepthPos_ = bitdepth;
  }

  void quantizeOpacity( const int32_t bitdepth ) {
    if ( bitdepth >= 32 ) return;
    double minBound = (double)gaussianData_[0].opacity_;
    double maxBound = (double)gaussianData_[0].opacity_;
    for ( const auto& data : gaussianData_ ) {
      minBound = std::min( minBound, (double)data.opacity_ );
      maxBound = std::max( maxBound, (double)data.opacity_ );
    }
    const double maxLevel = static_cast<double>( ( 1 << bitdepth ) - 1 );
    const double scale    = maxLevel / (double)( maxBound - minBound );
    for ( auto& data : gaussianData_ ) {
      uint32_t quantized = uint32_t( floor( ( data.opacity_ - minBound ) * scale + 0.5 ) );
      data.opacity_      = (float)( minBound + double( quantized ) / scale );
    }
    bitdepthOpacity_ = bitdepth;
  }

  void quantizeScale( const int32_t bitdepth ) {
    if ( bitdepth >= 32 ) return;
    glm::dvec3 minBound = glm::dvec3( gaussianData_[0].scale_ );
    glm::dvec3 maxBound = glm::dvec3( gaussianData_[0].scale_ );
    for ( const auto& data : gaussianData_ ) {
      minBound = glm::min( minBound, glm::dvec3( data.scale_ ) );
      maxBound = glm::max( maxBound, glm::dvec3( data.scale_ ) );
    }
    const double     maxLevel = static_cast<double>( ( 1 << bitdepth ) - 1 );
    const glm::dvec3 scale    = maxLevel / glm::dvec3( maxBound - minBound );
    for ( auto& data : gaussianData_ ) {
      glm::ivec3 quantized = glm::ivec3( glm::floor( ( glm::dvec3( data.scale_ ) - minBound ) * scale + 0.5 ) );
      data.scale_          = glm::vec3( minBound + glm::dvec3( quantized ) / scale );
    }
    bitdepthScale_ = bitdepth;
  }

  void quantizeRotate( const int32_t bitdepth ) {
    if ( bitdepth >= 32 ) return;
    if ( gaussianData_.empty() ) {
      std::cerr << "Erreur : le vecteur gaussianData_ est vide." << std::endl;
      return;
    }
    glm::dvec4 minBound = glm::dvec4( gaussianData_[0].rotate_.x, gaussianData_[0].rotate_.y,
                                      gaussianData_[0].rotate_.z, gaussianData_[0].rotate_.w );
    glm::dvec4 maxBound = minBound;
    for ( const auto& data : gaussianData_ ) {
      glm::dvec4 rotateVec = glm::dvec4( data.rotate_.x, data.rotate_.y, data.rotate_.z, data.rotate_.w );
      minBound             = glm::min( minBound, rotateVec );
      maxBound             = glm::max( maxBound, rotateVec );
    }
    const double     maxLevel = static_cast<double>( ( 1 << bitdepth ) - 1 );
    const glm::dvec4 scale    = maxLevel / ( maxBound - minBound );
    for ( auto& data : gaussianData_ ) {
      glm::dvec4 rotateVec = glm::dvec4( data.rotate_.x, data.rotate_.y, data.rotate_.z, data.rotate_.w );
      glm::ivec4 quantized = glm::ivec4( glm::floor( ( rotateVec - minBound ) * scale + 0.5 ) );
      rotateVec            = minBound + glm::dvec4( quantized ) / scale;
      data.rotate_         = glm::quat( rotateVec.w, rotateVec.x, rotateVec.y, rotateVec.z );
    }
    bitdepthRotate_ = bitdepth;
  }

  void quantizeSphericalHarmonics( const int32_t bitdepth ) {
    if ( bitdepth >= 32 ) return;
    for ( size_t i = 0; i < 3; i++ ) {
      for ( size_t j = 0; j < 3; j++ ) {
        glm::dvec4 minBound = gaussianData_[0].sh_[i][j];
        glm::dvec4 maxBound = gaussianData_[0].sh_[i][j];
        for ( const auto& data : gaussianData_ ) {
          minBound = glm::min( minBound, glm::dvec4( data.sh_[i][j] ) );
          maxBound = glm::max( maxBound, glm::dvec4( data.sh_[i][j] ) );
        }
        const double     maxLevel = static_cast<double>( ( 1 << bitdepth ) - 1 );
        const glm::dvec4 scale    = maxLevel / ( maxBound - minBound );
        for ( auto& data : gaussianData_ ) {
          glm::ivec4 quantized = glm::ivec4( glm::floor( ( glm::dvec4( data.sh_[i][j] ) - minBound ) * scale + 0.5 ) );
          data.sh_[i][j]       = glm::vec4( minBound + glm::dvec4( quantized ) / scale );
        }
      }
    }
    bitdepthSH_ = bitdepth;
  }

  void trace() {
    printf( "trace: num points = %d \n", size() );
    size_t modulo = 10000;
    int    num = 10, idx = 0;
    for ( int32_t i = 0; i < size() && idx < num; i += modulo, idx++ ) {
      auto& gs = gaussianData_[i];
      printf( "%7d: %6.2f %6.2f %6.2f ", i, gs.xyz_[0], gs.xyz_[1], gs.xyz_[2] );
      printf( "o: %6.2f ", gs.opacity_ );
      printf( "s: %6.2f %6.2f %6.2f ", gs.scale_[0], gs.scale_[1], gs.scale_[2] );
      printf( "r: %6.2f %6.2f %6.2f %6.2f ", gs.rotate_[0], gs.rotate_[1], gs.rotate_[2], gs.rotate_[3] );
      printf( "dc: %6.2f %6.2f %6.2f ", gs.sh_[0][0].x, gs.sh_[1][0].x, gs.sh_[2][0].x );
      printf( "rest: %6.2f %6.2f %6.2f ", gs.sh_[0][0].y, gs.sh_[1][0].y, gs.sh_[2][0].y );
      printf( " %6.2f %6.2f %6.2f ", gs.sh_[0][0].z, gs.sh_[1][0].z, gs.sh_[2][0].z );
      printf( "\n" );
    }
    auto [minBound, maxBound] = getBoundingBox();
    printf( "Bounding Box = ( %6.2f %6.2f %6.2f ) ( %6.2f %6.2f %6.2f ) \n", minBound.x, minBound.y, minBound.z,
            maxBound.x, maxBound.y, maxBound.z );
    fflush( stdout );
  }

  void diff( const Pointcloud& pc ) const {
    bool diff = false;
    if ( pc.size() != size() ) printf( "Diff: size are not the same \n" );
    for ( size_t i = 0; i < gaussianData_.size(); i++ )
      if ( gaussianData_[i].xyz_ != pc.gaussianData_[i].xyz_ ) {
        printf( "Diff pos %zu / %zu : \n", i, gaussianData_.size() );
        fflush( stdout );
        diff = true;
      }
    // for ( size_t i = 0; i < gaussianData_.size(); i++ )
    //   if ( gaussianData_[i].normal_ != pc.gaussianData_[i].normal_ ) {
    //     printf( "Diff normal %zu / %zu : \n", i, gaussianData_.size() );
    //     fflush( stdout );
    //     diff = true;
    //   }
    for ( size_t i = 0; i < gaussianData_.size(); i++ )
      if ( gaussianData_[i].opacity_ != pc.gaussianData_[i].opacity_ ) {
        printf( "Diff opacity %zu / %zu : \n", i, gaussianData_.size() );
        fflush( stdout );
        diff = true;
      }
    for ( size_t i = 0; i < gaussianData_.size(); i++ )
      if ( gaussianData_[i].scale_ != pc.gaussianData_[i].scale_ ) {
        printf( "Diff scale %zu / %zu : \n", i, gaussianData_.size() );
        fflush( stdout );
        diff = true;
      }
    for ( size_t i = 0; i < gaussianData_.size(); i++ )
      if ( gaussianData_[i].rotate_ != pc.gaussianData_[i].rotate_ ) {
        printf( "Diff rotate %zu / %zu : \n", i, gaussianData_.size() );
        fflush( stdout );
        diff = true;
      }

    for ( size_t i = 0; i < gaussianData_.size(); i++ )
      for ( size_t j = 0; j < 3; j++ )
        for ( size_t k = 0; k < 4; k++ )
          if ( gaussianData_[i].sh_[j][k] != pc.gaussianData_[i].sh_[j][k] ) {
            printf( "Diff sh %zu / %zu : \n", i, gaussianData_.size() );
            fflush( stdout );
            gaussianData_[i].traceSH();
            pc.gaussianData_[i].traceSH();
            diff = true;
            return;
          }

    if ( !diff ) {
      printf( "Not difference \n" );
      fflush( stdout );
    }
  }

  int32_t bitdepthPos() { return bitdepthPos_; }
  int32_t bitdepthOpacity() { return bitdepthOpacity_; }
  int32_t bitdepthScale() { return bitdepthScale_; }
  int32_t bitdepthRotate() { return bitdepthRotate_; }
  int32_t bitdepthSH() { return bitdepthSH_; }
  int32_t numSH() { return numSH_; }

  int32_t getNumBits() {
    return 3 * bitdepthPos_ +      //
           1 * bitdepthOpacity_ +  //
           3 * bitdepthScale_ +    //
           4 * bitdepthRotate_ +   //
           numSH_ * 3 * bitdepthSH_;
  }

  std::string getParameters() {
    return stringFormat( "pos=%d op=%2d scale=%2d rot=%2d sh=%2d numSh=%2d", bitdepthPos_, bitdepthOpacity_,
                         bitdepthScale_, bitdepthRotate_, bitdepthSH_, numSH_ );
  }

  std::string getName() {
    return stringFormat( "pos=%2d_op=%02d_scale=%02d_rot=%02d_sh=%02d_numSh=%02d", bitdepthPos_, bitdepthOpacity_,
                         bitdepthScale_, bitdepthRotate_, bitdepthSH_, numSH_ );
  }

 private:
  std::vector<GaussianData> gaussianData_;

  int32_t bitdepthPos_     = 32;
  int32_t bitdepthOpacity_ = 32;
  int32_t bitdepthScale_   = 32;
  int32_t bitdepthRotate_  = 32;
  int32_t bitdepthSH_      = 32;
  int32_t numSH_           = 16;
};
