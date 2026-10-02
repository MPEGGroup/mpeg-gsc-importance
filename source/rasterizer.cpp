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

#include "rasterizer.hpp"
#include "camera.hpp"
#include "pointcloud.hpp"
#include "viewpoints.hpp"

#include <smmintrin.h>

namespace {

constexpr float kAlphaThreshold = 1.0f / 255.0f;

inline float deterministic_sqrt(float x) {
  __m128 v = _mm_set_ss(x);
  return _mm_cvtss_f32(_mm_sqrt_ss(v));
}

inline float deterministic_ceil(float x) {
  __m128 v = _mm_set_ss(x);
  return _mm_cvtss_f32(_mm_round_ss(v, v, _MM_FROUND_TO_POS_INF | _MM_FROUND_NO_EXC));
}

inline float deterministic_floor(float x) {
  __m128 v = _mm_set_ss(x);
  return _mm_cvtss_f32(_mm_round_ss(v, v, _MM_FROUND_TO_NEG_INF | _MM_FROUND_NO_EXC));
}

inline float deterministic_log(float x) {
  if (x <= 0.0f) return -87.3365f;
  uint32_t ix;
  std::memcpy(&ix, &x, sizeof(float));
  int iexp = static_cast<int>((ix >> 23) & 0xFF) - 126;
  uint32_t mx = (ix & 0x007FFFFF) | 0x3F000000;
  float luxury_u;
  std::memcpy(&luxury_u, &mx, sizeof(float));
  float s = (luxury_u - 1.0f) / (luxury_u + 1.0f);
  float s2 = s * s;
  float r = s * (2.0000000f + s2 * (0.6666667f + s2 * (0.4000000f + s2 * 0.2857143f)));
  return r + static_cast<float>(iexp) * 0.69314718f;
}

inline glm::vec3 deterministic_normalize(const glm::vec3& v) {
  float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
  if (lenSq > 0.0f) {
    return v * (1.0f / deterministic_sqrt(lenSq));
  }
  return v;
}

template <typename T>
T clamp( T value, T minimum, T maximum ) {
  return value < minimum ? minimum : ( value > maximum ? maximum : value );
}

glm::mat3 quatToRotation( glm::quat q ) {
  float xx = q.x * q.x, xy = q.x * q.y, xz = q.x * q.z;
  float yy = q.y * q.y, yz = q.y * q.z, zz = q.z * q.z;
  float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
  return glm::transpose( glm::mat3( 1. - 2. * ( yy + zz ), 2. * ( xy - wz ), 2. * ( xz + wy ), 2. * ( xy + wz ),
                                    1. - 2. * ( xx + zz ), 2. * ( yz - wx ), 2. * ( xz - wy ), 2. * ( yz + wx ),
                                    1. - 2. * ( xx + yy ) ) );
}

glm::mat3 computeCovariance3D( glm::quat quat, glm::vec4 scale ) {
  glm::mat3 R = quatToRotation( quat );
  glm::mat3 M = glm::mat3( R[0] * scale.x, R[1] * scale.y, R[2] * scale.z );
  return M * glm::transpose( M );
}

glm::mat2 computeCovariance2D( glm::vec3 mean, glm::mat3 covar, glm::mat3 viewMat, glm::vec2 focal, glm::vec2 window ) {
  glm::vec2   center = window * 0.5f;
  glm::vec2   limit  = 1.3f * center / focal;
  float       rz     = 1.0f / mean.z;
  float       rz2    = rz * rz;
  glm::vec2   t      = mean.z * clamp( mean.xy() * rz, -limit, limit );
  glm::mat3x2 J      = glm::mat3x2( -focal.x * rz, 0.0, 0.0, -focal.y * rz, -focal.x * t.x * rz2, focal.y * t.y * rz2 );
  glm::mat3   cov    = viewMat * covar * glm::transpose( viewMat );
  cov = glm::mat3( cov[0][0], -cov[0][1], -cov[0][2], -cov[1][0], cov[1][1], cov[1][2], -cov[2][0], cov[2][1],
                   cov[2][2] );
  return J * cov * transpose( J );
}

glm::vec3 computeConic( glm::mat2 covar2d, float &radius ) {
  covar2d[0][0] += 0.3;
  covar2d[1][1] += 0.3;
  float det = std::max( covar2d[0][0] * covar2d[1][1] - covar2d[0][1] * covar2d[1][0], 1e-10f );
  float b   = 0.5f * ( covar2d[0][0] + covar2d[1][1] );
  float s   = deterministic_sqrt( std::max( b * b - det, 0.1f ) );
  radius    = deterministic_ceil( 3.0f * deterministic_sqrt( std::max( b - s, b + s ) ) );
  return glm::vec3( covar2d[1][1] / det, -( covar2d[0][1] + covar2d[1][0] ) / ( 2.0 * det ), covar2d[0][0] / det );
}

void accumulateTransmittanceSums( const Pointcloud            &pc,
                                  const glm::mat4             &viewMatrix,
                                  const glm::mat4             &projectionMatrix,
                                  const glm::vec2             &focal,
                                  const glm::vec2             &window,
                                  const glm::vec2             &halfWindow,
                                  const std::vector<uint32_t> &sorted,
                                  std::vector<float>          &transmittanceSums ) {
  const int32_t        width  = static_cast<int32_t>( window.x );
  const int32_t        height = static_cast<int32_t>( window.y );
  std::vector<float>   transmittance( static_cast<size_t>( width ) * static_cast<size_t>( height ), 1.0f );
  std::vector<uint8_t> done( static_cast<size_t>( width ) * static_cast<size_t>( height ), 0 );
  const glm::mat3      viewMatrix3( viewMatrix );

  for ( const auto &pointIndex : sorted ) {
    float       radius;
    const auto &gaussian     = pc[pointIndex];
    glm::vec4   viewPosition = viewMatrix * gaussian.xyz_;
    glm::vec4   clipPosition = projectionMatrix * viewPosition;
    glm::vec2   normalizedPosition( clipPosition.x / clipPosition.w, clipPosition.y / clipPosition.w );
    glm::mat3   covariance3D = computeCovariance3D( gaussian.rotate_, gaussian.scale_ );
    glm::mat2   covariance2D = computeCovariance2D( viewPosition.xyz(), covariance3D, viewMatrix3, focal, window );
    glm::vec3   conic        = computeConic( covariance2D, radius );
    glm::vec2   screen       = halfWindow * normalizedPosition + halfWindow;
    float       opacity      = gaussian.opacity_;
    if ( opacity <= kAlphaThreshold ) continue;

    float       sigmaThreshold = deterministic_log( opacity / kAlphaThreshold );
    const float sigmaXX        = 0.5f * conic.x;
    const float sigmaYY        = 0.5f * conic.z;
    const float sigmaXY        = -conic.y;
    float       radiusU        = radius;
    float       radiusV        = radius;
    float       determinant    = sigmaXX * sigmaYY - 0.25f * sigmaXY * sigmaXY;
    if ( determinant > 1e-10f ) {
      radiusU = std::min( radiusU, deterministic_sqrt( std::max( sigmaThreshold * sigmaYY / determinant, 0.0f ) ) );
      radiusV = std::min( radiusV, deterministic_sqrt( std::max( sigmaThreshold * sigmaXX / determinant, 0.0f ) ) );
    }

    int32_t minimumU = std::max( static_cast<int32_t>( deterministic_floor( screen.x - radiusU ) ), 0 );
    int32_t minimumV = std::max( static_cast<int32_t>( deterministic_floor( screen.y - radiusV ) ), 0 );
    int32_t maximumU = std::min( static_cast<int32_t>( deterministic_ceil( screen.x + radiusU ) ), width - 1 );
    int32_t maximumV = std::min( static_cast<int32_t>( deterministic_ceil( screen.y + radiusV ) ), height - 1 );

    for ( int32_t vertical = minimumV; vertical <= maximumV; ++vertical ) {
      size_t row           = static_cast<size_t>( vertical ) * static_cast<size_t>( width );
      float  deltaY        = screen.y - static_cast<float>( vertical + 0.5f );
      float  rowCross      = sigmaXY * deltaY;
      float  rowQuadratic  = sigmaYY * deltaY * deltaY;
      float  initialDeltaX = screen.x - static_cast<float>( minimumU + 0.5f );
      float  sigma         = sigmaXX * initialDeltaX * initialDeltaX + rowCross * initialDeltaX + rowQuadratic;
      float  sigmaDelta    = -2.0f * sigmaXX * initialDeltaX + sigmaXX - rowCross;

      for ( int32_t horizontal = minimumU; horizontal <= maximumU; ++horizontal ) {
        size_t pixelIndex = row + static_cast<size_t>( horizontal );
        float  sigmaValue = sigma;
        sigma += sigmaDelta;
        sigmaDelta += 2.0f * sigmaXX;
        if ( done[pixelIndex] != 0 || sigmaValue < 0.0f || sigmaValue > sigmaThreshold ) continue;

        float alpha = std::min( opacity * deterministic_exp( -sigmaValue ), 0.999f );
        if ( alpha < kAlphaThreshold ) continue;

        float currentTransmittance = transmittance[pixelIndex];
        float nextTransmittance    = currentTransmittance * ( 1.0f - alpha );
        if ( nextTransmittance <= 1e-4f ) {
          done[pixelIndex] = 1;
        } else {
          float visibility = alpha * currentTransmittance;
          transmittanceSums[pointIndex] += visibility * visibility;
          transmittance[pixelIndex] = nextTransmittance;
        }
      }
    }
  }
}

}  // namespace

void Rasterizer::computeTransmittanceSums( const Pointcloud   &pc,
                                           const Viewpoint    &viewpoint,
                                           std::vector<float> &transmittanceSums ) {
  int32_t width  = viewpoint.originalWidth();
  int32_t height = viewpoint.originalHeight();
  auto    focal  = viewpoint.focal();
  auto    center = viewpoint.center();
  if ( viewpoint.originalHeight() > 0 ) {
    float scale = static_cast<float>( height ) / static_cast<float>( viewpoint.originalHeight() );
    focal       = focal * scale;
    center      = center * scale;
  }
  
  Camera camera;
  camera.initialize( width, height );

  glm::vec3 deterministic_dir = viewpoint.view() - viewpoint.pos();
  glm::vec3 f = deterministic_normalize(deterministic_dir);
  glm::vec3 s = deterministic_normalize(glm::cross(f, viewpoint.up()));
  glm::vec3 u = glm::cross(s, f);

  float     zNear = 0.01f, zFar = 10000000000.0f;
  glm::mat4 projMat;
  if ( center.x > 0.0f && center.y > 0.0f && focal.x > 0.0f && focal.y > 0.0f ) {
    float left  = -zNear * center.x / focal.x;
    float right = zNear * ( static_cast<float>( width ) - center.x ) / focal.x;
    float bottom = -zNear * ( static_cast<float>( height ) - center.y ) / focal.y;
    float top    = zNear * center.y / focal.y;
    projMat = glm::frustum( left, right, bottom, top, zNear, zFar );
  } else {
    float fov = 2.0f * atan( 1.0f / ( 2.0f * focal[1] / static_cast<float>( height ) ) );
    camera.create( 10.0f, fov );
    projMat = camera.projMat();
  }

  glm::mat4 viewMat(1.0f);
  viewMat[0][0] = s.x;  viewMat[1][0] = s.y;  viewMat[2][0] = s.z;
  viewMat[0][1] = u.x;  viewMat[1][1] = u.y;  viewMat[2][1] = u.z;
  viewMat[0][2] = -f.x; viewMat[1][2] = -f.y; viewMat[2][2] = -f.z;
  viewMat[3][0] = -glm::dot(s, viewpoint.pos());
  viewMat[3][1] = -glm::dot(u, viewpoint.pos());
  viewMat[3][2] =  glm::dot(f, viewpoint.pos());

  auto            nearFar          = camera.nearFar();
  const glm::vec2 window( width, height );
  const glm::vec2 halfWindow( width * 0.5f, height * 0.5f );
  auto            sorted = pc.sort( projMat * viewMat, nearFar, true );

  accumulateTransmittanceSums( pc, viewMat, projMat, focal, window, halfWindow, sorted, transmittanceSums );
}