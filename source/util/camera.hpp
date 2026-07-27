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
  Camera() = default;

  void initialize( int width, int height ) { ratio_ = static_cast<float>( width ) / static_cast<float>( height ); }

  void create( float boxSize, float fov ) {
    rotation_ = glm::quat( 1, 0, 0, 0 );
    center_   = glm::vec3( boxSize / 2.f );
    dist_     = boxSize;
    fov_      = fov;
  }

  glm::mat4 viewMat() const {
    glm::mat3 mat = glm::toMat3( rotation_ );
    return glm::lookAt( mat * glm::vec3( 0, 0, dist_ ) + center_, center_, mat * glm::vec3( 0, 1, 0 ) );
  }
  auto nearFar() const { return nearFar_; }

  glm::mat4 projMat() const { return glm::perspective( fov_, ratio_, nearFar_.x, nearFar_.y ); }

  void setLookAt( glm::vec3 position, glm::vec3 center, glm::vec3 up ) {
    glm::vec3 zAxis = glm::normalize( position - center );
    glm::vec3 xAxis = glm::normalize( glm::cross( up, zAxis ) );
    glm::vec3 yAxis = glm::normalize( glm::cross( zAxis, xAxis ) );
    rotation_       = glm::toQuat( glm::mat3( xAxis, yAxis, zAxis ) );
    center_         = center;
    dist_           = glm::length( position - center );
  }

 private:
  glm::quat rotation_ = glm::quat( 1, 0, 0, 0 );
  glm::vec3 center_   = glm::vec3( 0 );
  float     dist_     = 1.0f;
  float     ratio_    = 1.0f;
  float     fov_      = glm::radians( 45.0f );
  glm::vec2 nearFar_  = { 0.010000f, 10000000000.0f };
};