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

class Camera {
 public:
  Camera( int width = 0, int height = 0, float fov = glm::radians( 45.f ) ) :
      rotation_( 1, 0, 0, 0 ), center_( 0, 0, 0 ), dist_( 1.0f ), boxSize_( 1.0f ), fov_( fov ) {
    initialize( width, height );
  }
  Camera( int width, int height, float fov, const glm::vec3 ePos, const glm::vec3 eCenter, const glm::vec3 eUp ) :
      rotation_( 1, 0, 0, 0 ), center_( 0, 0, 0 ), dist_( 1.0f ), boxSize_( 1.0f ), fov_( fov ) {
    initialize( width, height );
    setLookAt( ePos, eCenter, eUp );
  }

  void initialize( int width, int height ) { ratio_ = (float)width / (float)height; }

  void create( const float fBoxSize, const float fFov ) {
    rotation_ = glm::quat( 1, 0, 0, 0 );
    center_   = glm::vec3( fBoxSize / 2.f, fBoxSize / 2.f, fBoxSize / 2.f );
    dist_     = fBoxSize;
    boxSize_  = fBoxSize;
    fov_      = fFov;
  }

  glm::vec3 center() { return center_; }
  glm::vec3 getPosition() const { return glm::toMat3( rotation_ ) * glm::vec3( 0, 0, dist_ ) + center_; }

  glm::mat4 viewMat() {
    glm::mat3 mat = glm::toMat3( rotation_ );
    return glm::lookAt( mat * glm::vec3( 0, 0, dist_ ) + center_, center_, mat * glm::vec3( 0, 1, 0 ) );
  }
  auto nearFar() const { return nearFar_; }

  glm::mat4 projMat() const { return glm::perspective( fov_, ratio_, nearFar_.x, nearFar_.y ); }

  std::tuple<glm::vec3, glm::vec3, glm::vec3> getLookAt() {
    auto mat = glm::toMat3( rotation_ );
    return std::make_tuple( mat * glm::vec3( 0, 0, dist_ ) + center_, center_, mat * glm::vec3( 0, 1, 0 ) );
  }

  void setLookAt( const glm::vec3 ePos, const glm::vec3 eCenter, const glm::vec3 eUp ) {
    glm::vec3 eZ = glm::normalize( ePos - eCenter );
    glm::vec3 eX = glm::normalize( glm::cross( eUp, eZ ) );
    glm::vec3 eY = glm::normalize( glm::cross( eZ, eX ) );
    rotation_    = glm::toQuat( glm::mat3( eX, eY, eZ ) );
    center_      = eCenter;
    dist_        = glm::length( ePos - eCenter );
  }

  glm::vec3& projectToSphere( const float& fRadius, glm::vec3& ePoint ) const {
    const float fD = ePoint[0] * ePoint[0] + ePoint[1] * ePoint[1], fR = fRadius * fRadius;
    if ( fD < fR ) {
      ePoint[2] = std::sqrt( fR - fD );
    } else {
      ePoint[2] = 0;
      ePoint *= fRadius / glm::length( ePoint );
    }
    return ePoint;
  }

  glm::quat getRotate( const double fX0, const double fY0, const double fX1, const double fY1 ) {
    glm::quat ret( 1, 0, 0, 0 );
    glm::vec3 eP0( static_cast<float>( fX0 ), static_cast<float>( fY0 ), 0 );
    glm::vec3 eP1( static_cast<float>( fX1 ), static_cast<float>( fY1 ), 0 );
    if ( glm::length( eP0 - eP1 ) >= 1.0e-16f ) {
      glm::vec3 eV0 = glm::normalize( projectToSphere( 0.9f, eP1 ) );
      glm::vec3 eV1 = glm::normalize( projectToSphere( 0.9f, eP0 ) );
      float     fC  = glm::dot( eV1, eV0 );
      if ( fC > -1 ) {
        float fS = std::sqrt( ( 1 + fC ) * 2 );
        eV0      = glm::cross( eV0, eV1 ) * ( 1.f / fS );
        ret      = glm::quat( fS * 0.5f, eV0[0], eV0[1], eV0[2] );
      }
    }
    return ret;
  }

  void rotate( const double fX0, const double fY0, const double fX1, const double fY1 ) {
    rotation_ *= getRotate( fX0, fY0, fX1, fY1 );
    std::tuple<glm::vec3, glm::vec3, glm::vec3> value = getLookAt();
    setLookAt( std::get<0>( value ), std::get<1>( value ), std::get<2>( value ) );
  }

  void translate( float x, float y, float z ) { center_ += glm::toMat3( rotation_ ) * glm::vec3( x, y, z ); }

  void zoom( float fZoom ) {
    if ( fZoom > 0 ) {
      dist_ *= 1.f + fZoom / 1500.f;
      if ( dist_ > 40.f * boxSize_ ) { dist_ = 40.f * boxSize_; }
    } else {
      dist_ /= 1.f - fZoom / 1500.f;
      if ( dist_ < 0.05f * boxSize_ ) { dist_ = 0.05f * boxSize_; }
    }
  }

 private:
  glm::quat rotation_;
  glm::vec3 center_;
  float     dist_;
  float     boxSize_;
  float     ratio_;
  float     fov_     = glm::radians( 45.0f );
  glm::vec2 nearFar_ = { 0.010000f, 10000000000.0f };
};