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
#include "rasterizerSoftware.hpp"
#include "pointcloud.hpp"
#include "image.hpp"
#include "viewpoints.hpp"

RasterizerSoftware::RasterizerSoftware() {}

RasterizerSoftware::~RasterizerSoftware() {}

inline glm::mat2 invMat( glm::mat2& m ) {
  float det = m[0][0] * m[1][1] - m[0][1] * m[1][0];
  return glm::mat2( m[1][1] / det, -m[0][1] / det, -m[1][0] / det, m[0][0] / det );
}

inline glm::mat3 quatToMat( glm::quat q ) {
  return glm::mat3( 1 - 2 * ( q.y * q.y + q.z * q.z ), 2 * ( q.x * q.y + q.w * q.z ), 2 * ( q.x * q.z - q.w * q.y ),
                    2 * ( q.x * q.y - q.w * q.z ), 1 - 2 * ( q.x * q.x + q.z * q.z ), 2 * ( q.y * q.z + q.w * q.x ),
                    2 * ( q.x * q.z + q.w * q.y ), 2 * ( q.y * q.z - q.w * q.x ), 1 - 2 * ( q.x * q.x + q.y * q.y ) );
}

constexpr double radians( double degrees ) { return degrees * M_PI / 180.0; }

template <typename T, int N>
void RasterizerSoftware::render( const Pointcloud& pc, const Viewpoint& viewpoint, Image<T, N>& image ) {
  int       width     = image.width();
  int       height    = image.height();
  int32_t   numPoints = pc.size();
  int32_t   count     = 0;
  Camera    camera( image.width(), image.height(), viewpoint.pos(), viewpoint.view(), viewpoint.up() );
  glm::mat4 viewMat = camera.viewMat();
  auto      pos     = camera.center();
  auto      projMat = camera.projMat();
  auto      nearFar = camera.nearFar();

  // Sort
  std::vector<uint32_t> depth( numPoints );
  std::vector<uint32_t> index( numPoints );
  const uint32_t        maxValue      = std::numeric_limits<uint32_t>::max();
  auto                  modelViewProj = projMat * viewMat;
  for ( int32_t i = 0; i < numPoints; ++i ) {
    glm::vec4 p = modelViewProj * glm::vec4( pc[i].xyz_, 1.f );
    if ( p.w > nearFar.x && p.w < nearFar.y && std::abs( p.x / p.w ) < 1.2 && std::abs( p.y / p.w ) < 1.2 ) {
      depth[count] = uint32_t( (float)maxValue * ( nearFar.y - p.w ) / ( nearFar.y - nearFar.x ) );
      index[count] = i;
      count++;
      // if ( count > 100 ) break;
    }
  }
  std::vector<uint32_t> indices( count );
  std::iota( indices.begin(), indices.end(), 0 );
  std::sort( indices.begin(), indices.end(), [&]( uint32_t a, uint32_t b ) { return depth[a] < depth[b]; } );
  std::vector<uint32_t> sorted( count );
  std::transform( indices.begin(), indices.end(), sorted.begin(), [&]( uint32_t idx ) { return index[idx]; } );

  // Project sorted splat
  Image<glm::vec4, 1> buffer( width, height, glm::vec4( 0 ) );
  for ( const auto& i : sorted ) {
    const auto& gs = pc[i];
    // Vertex shader
    glm::vec4 t = viewMat * glm::vec4( gs.xyz_, 1.0 );
    glm::vec4 p = projMat * t;
    glm::mat3 J = glm::mat3(
        glm::vec3( -projMat[0][0] * width / ( 2.0 * t.z ), 0.0, projMat[0][0] * width * t.x / ( 2.0 * t.z * t.z ) ),
        glm::vec3( 0.0, -projMat[1][1] * height / ( 2.0 * t.z ), projMat[1][1] * height * t.y / ( 2.0 * t.z * t.z ) ),
        glm::vec3( 0.0, 0.0, ( nearFar.y - nearFar.x ) * projMat[3][2] / ( 2.0 * t.z * t.z ) ) );
    glm::mat3 R      = quatToMat( glm::normalize( gs.rotate_ ) );
    glm::mat3 S      = glm::mat3( glm::vec3( exp( gs.scale_.x ), 0.0, 0.0 ), glm::vec3( 0.0, exp( gs.scale_.y ), 0.0 ),
                             glm::vec3( 0.0, 0.0, exp( gs.scale_.z ) ) );
    glm::mat3 C      = R * S * glm::transpose( S ) * glm::transpose( R );
    glm::mat3 JW     = J * glm::mat3( viewMat );
    glm::mat3 V      = JW * C * glm::transpose( JW );
    glm::mat2 cov    = glm::mat2( V[0][0], V[0][1], V[1][0], V[1][1] ) + glm::mat2( 0.6, 0.0, 0.0, 0.6 );
    glm::mat2 covInv = invMat( cov );
    glm::vec2 screen = glm::vec2( 0.5f * width * ( p.x / p.w + 1 ) - 0.5, 0.5f * height * ( p.y / p.w + 1 ) - 0.5 );
    glm::vec4 color  = glm::clamp( glm::vec4( gs.computeRadiance( gs.xyz_ - pos ), gs.alpha() ) );

    // Geometry shader
    glm::vec3 ndcP = glm::vec3( p.x, p.y, p.z ) / p.w;
    if ( ndcP.z < 0.25f || ndcP.x > 2.0f || ndcP.x < -2.0f || ndcP.y > 2.0f || ndcP.y < -2.0f ) { continue; }
    float avg   = ( cov[0][0] + cov[1][1] ) * 0.5;
    float diff  = ( cov[0][0] - cov[1][1] ) * 0.5;
    float diag  = sqrt( diff * diff + cov[0][1] * cov[0][1] );
    float angle = ( cov[0].y == 0.0 ) ? ( cov[0][0] >= cov[1][1] ? 0.0 : radians( 90.0 ) )
                                      : glm::atan( avg + diag - cov[0][0], cov[0][1] );
    //    glm::vec2 a0    = 3.5f * sqrt( avg + diag ) * glm::vec2( cos( angle ), sin( angle ) );
    glm::vec2 a0 = 3.5f * glm::vec2( sqrt( avg + diag ) ) * glm::vec2( cos( angle ), sin( angle ) );
    glm::vec2 a1 = 3.5f * glm::vec2( sqrt( avg - diag ) ) * glm::vec2( -sin( angle ), cos( angle ) );

    // Frament shader
    glm::vec2 P[4] = { screen + a0 + a1, screen - a0 + a1, screen + a0 - a1, screen - a0 - a1 };
    int32_t   minU = std::max( (int)( std::floor( std::min( { P[0].x, P[1].x, P[2].x, P[3].x } ) ) ), 0 );
    int32_t   minV = std::max( (int)( std::floor( std::min( { P[0].y, P[1].y, P[2].y, P[3].y } ) ) ), 0 );
    int32_t   maxU = std::min( (int)( std::ceil( std::max( { P[0].x, P[1].x, P[2].x, P[3].x } ) ) ), width - 1 );
    int32_t   maxV = std::min( (int)( std::ceil( std::max( { P[0].y, P[1].y, P[2].y, P[3].y } ) ) ), height - 1 );
    for ( int32_t v = minV; v <= maxV; ++v ) {
      for ( int32_t u = minU; u <= maxU; ++u ) {
        glm::vec2 d = screen - glm::vec2( u, v );
        float     g = exp( -0.5f * glm::dot( d, covInv * d ) );
        if ( g * color.a > 1.0f / 256.0f ) {
          glm::vec4 out_color = glm::vec4( color.rgb() * color.a * g, color.a * g );
          auto&     value     = buffer[0].get( u, v );
          value               = glm::vec4( out_color.rgb() + value.rgb() * ( 1.0f - out_color.a ),
                             out_color.a + value.a * ( 1.0f - out_color.a ) );
        }
      }
    }
  }

  // Convert vec4 image to output image
  const int32_t size = width * height;
  if constexpr ( std::is_same_v<T, uint8_t> ) {
    const float scale = 255.f;
    for ( int i = 0; i < size; ++i ) {
      auto& current     = buffer[0].get( i );
      image[0].get( i ) = (T)roundf( current.r * scale );
      image[1].get( i ) = (T)roundf( current.g * scale );
      image[2].get( i ) = (T)roundf( current.b * scale );
      if ( N == 4 ) image[3].get( i ) = ( T )( current.a * scale );
    }
  } else if constexpr ( std::is_same_v<T, uint16_t> ) {
    const float scale = 65535.f;
    for ( int i = 0; i < size; ++i ) {
      auto& current     = buffer[0].get( i );
      image[0].get( i ) = (T)roundf( current.r * scale );
      image[1].get( i ) = (T)roundf( current.g * scale );
      image[2].get( i ) = (T)roundf( current.b * scale );
      if ( N == 4 ) image[3].get( i ) = ( T )( current.a * scale );
    }
  } else if constexpr ( std::is_same_v<T, float> ) {
    for ( int i = 0; i < size; ++i ) {
      auto& current     = buffer[0].get( i );
      image[0].get( i ) = current.r;
      image[1].get( i ) = current.g;
      image[2].get( i ) = current.b;
      if ( N == 4 ) image[3].get( i ) = ( T )( current.a );
    }
  } else {
    printf( "Error: template type not support in %s\n", __FILE__ );
    fflush( stdout );
    exit( -1 );
  }
}

template void RasterizerSoftware::render<uint8_t>( Pointcloud const&, const Viewpoint&, Image<uint8_t>& );
template void RasterizerSoftware::render<uint16_t>( Pointcloud const&, const Viewpoint&, Image<uint16_t>& );
template void RasterizerSoftware::render<float>( Pointcloud const&, const Viewpoint&, Image<float>& );