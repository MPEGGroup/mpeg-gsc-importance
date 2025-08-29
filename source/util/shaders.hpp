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

// clang-format off

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Splat shader ////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

// Splat Vertex Shader
static const std::string g_splatVertexShader = SHADERR( R"(
  precision highp float;
  uniform mat4 viewMat;  
  uniform mat4 projMat;
  uniform vec2 focal;
  uniform vec4 window;   
  uniform vec3 eye;
  struct GaussianData {
    vec4  pos;
    vec4  scale;
    vec4  rotate; 
    vec4  sh[16];
    float opacity;
  };
  layout(std430, binding = 0) buffer GaussianBuffer { GaussianData gaussians[]; };
  layout(std430, binding = 1) buffer IndexBuffer { uint indices[]; };
  out vec4 frag_color;
  out vec3 frag_conic;
  out vec2 frag_screen;

  // Constants
  const vec2 offsets[4] = vec2[]( vec2(1, 1), vec2(-1, 1), vec2(1, -1), vec2(-1, -1) );
  const float C[14] = float[]( 0.28209479177387814f,  0.4886025119029199f, 1.0925484305920792f, -1.0925484305920792f, 
                               0.31539156525252005f, -1.0925484305920792f, 0.5462742152960396f, -0.5900435899266435f, 
                               2.890611442640554f,   -0.4570457994644658f, 0.3731763325901154f, -0.4570457994644658f,
                               1.445305721320277f,   -0.5900435899266435f );
  vec4 computeRadiance(const vec4 sh[16], const vec3 direction, float opacity) {
    vec3  v    = normalize(direction); 
    float xx   = v.x * v.x, yy = v.y * v.y, zz = v.z * v.z, xy = v.x * v.y, yz = v.y * v.z, xz = v.x * v.z;
    vec3 color = C[ 0]                                             * sh[ 0].xyz;
    color     += C[ 1] * ( -v.y * sh[1].xyz + v.z * sh[2].xyz - v.x * sh[3].xyz );
    color     += C[ 2] * xy                                         * sh[ 4].xyz 
               + C[ 3] * yz                                         * sh[ 5].xyz
               + C[ 4] * (2.0f * zz - xx - yy)                      * sh[ 6].xyz 
               + C[ 5] * xz                                         * sh[ 7].xyz 
               + C[ 6] * (xx - yy)                                  * sh[ 8].xyz;
    color     += C[ 7] * v.y * (3.0f * xx - yy)                     * sh[ 9].xyz 
               + C[ 8] * xy * v.z                                   * sh[10].xyz 
               + C[ 9] * v.y * (4.0f * zz - xx - yy)                * sh[11].xyz 
               + C[10] * v.z * (2.0f * zz - 3.0f * xx - 3.0f * yy)  * sh[12].xyz 
               + C[11] * v.x * (4.0f * zz - xx - yy)                * sh[13].xyz 
               + C[12] * v.z * (xx - yy)                            * sh[14].xyz 
               + C[13] * v.x * (xx - 3.0f * yy)                     * sh[15].xyz;             
    color     += 0.5f; 
    color = max( color, 0.0f );
    return vec4( color, opacity);
  }

  mat3 quatToRotation(vec4 q) {
    float x2 = q.x + q.x, y2 = q.y + q.y, z2 = q.z + q.z;
    float xx = q.x * x2,  yy = q.y * y2,  zz = q.z * z2;
    float xy = q.x * y2,  xz = q.x * z2,  yz = q.y * z2;
    float wx = q.w * x2,  wy = q.w * y2,  wz = q.w * z2;
    return mat3( 1.0 - (yy + zz),  xy + wz, xz - wy, xy - wz, 1.0 - (xx + zz), yz + wx, xz + wy, yz - wx, 1.0 - (xx + yy) );
  }

  mat3 computeCovariance3D( vec4 quat, vec4 scale ){
    mat3 R = quatToRotation(quat);
    mat3 M = mat3( R[0] * scale.x, R[1] * scale.y, R[2] * scale.z );
    return M * transpose(M);
  }
 
  mat2 computeCovariance2D( vec3 mean, mat3 covar, mat3 viewMat, vec2 focal, vec2 window ) {
    vec2   center = window * 0.5;
    vec2   limit  = 1.3 * center / focal;
    float  rz     = 1.0 / mean.z;
    float  rz2    = rz * rz;
    vec2   t      = mean.z * clamp(mean.xy * rz, -limit, limit);
    mat3x2 J      = mat3x2( -focal.x * rz, 0.0, 0.0, -focal.y * rz, -focal.x * t.x * rz2,  focal.y * t.y * rz2 ); 
    mat3   cov    = viewMat * covar * transpose( viewMat );
    cov           = mat3( cov[0][0], -cov[0][1], -cov[0][2], -cov[1][0], cov[1][1], cov[1][2], -cov[2][0], cov[2][1], cov[2][2] );
    return J * cov * transpose( J );
  }

  vec3 computeConic( mat2 covar2d, out float radius ){
    covar2d[0][0] += 0.3;
    covar2d[1][1] += 0.3;
    float det = max(covar2d[0][0] * covar2d[1][1] - covar2d[0][1] * covar2d[1][0], 1e-10);
    float b   = 0.5f * ( covar2d[0][0] + covar2d[1][1] );
    float s   = sqrt( max( b * b - det, 0.1f ) );
    radius    = ceil( 3.0 * sqrt( max( b - s, b + s ) ) );
    return vec3( covar2d[1][1] / det, -(covar2d[0][1] + covar2d[1][0]) / (2.0 * det), covar2d[0][0] / det ); 
  }

  void main(void) {
    float radius;
    uint id      = indices[gl_InstanceID];
    vec4 t       = viewMat * gaussians[id].pos;
    vec4 p       = projMat * t;
    vec3 ndc     = p.xyz / p.w;
    mat3 covar3d = computeCovariance3D(gaussians[id].rotate, gaussians[id].scale);
    mat2 covar2d = computeCovariance2D(t.xyz, covar3d, mat3(viewMat), focal, window.xy);
    frag_conic   = computeConic(covar2d, radius);
    frag_color   = computeRadiance(gaussians[id].sh, gaussians[id].pos.xyz - eye, gaussians[id].opacity );
    frag_screen  = window.xy * 0.5 * (1.0 + ndc.xy); 
    gl_Position  = vec4(ndc.xy + offsets[gl_VertexID % 4] * 2 * radius / window.xy, ndc.z, 1.0);
  }    
)");

// Splat fragment shader
static const std::string g_splatFragmentShader = SHADERR(  R"(
  precision highp float;
  in  vec4 frag_color;
  in  vec3 frag_conic;
  in  vec2 frag_screen;
  out vec4 out_color;
  void main() {
    vec2  d     = gl_FragCoord.xy - frag_screen.xy;     
    float sigma = 0.5 * (frag_conic.x * d.x * d.x + frag_conic.z * d.y * d.y) - frag_conic.y * d.x * d.y; 
    if ( sigma < 0.0f )
      discard; 
    float alpha = min(frag_color.a * exp(-sigma), 0.999); 
    if (alpha  < 1.0f / 255.0f)
      discard;
    out_color = vec4( frag_color.rgb, alpha );
  }
)");

// Splat fragment shader with ocm 
static const std::string g_splatFragmentShaderWithOcm = SHADERR(  R"(
  precision highp float;
  in  vec4 frag_color;
  in  vec3 frag_conic;
  in  vec2 frag_screen;
  out vec4 out_color;  
  layout(r8, binding = 2) uniform writeonly image2D ocm;
  void main() {
    vec2  d     = gl_FragCoord.xy - frag_screen.xy;     
    float sigma = 0.5 * (frag_conic.x * d.x * d.x + frag_conic.z * d.y * d.y) - frag_conic.y * d.x * d.y; 
    if ( sigma < 0.0f )
      discard; 
    float alpha = min(frag_color.a * exp(-sigma), 0.999); 
    if (alpha < 1.0f / 255.0f)
      discard;
    out_color = vec4( frag_color.rgb, alpha );
    if ( alpha > 10.0 / 255.0 ) {
      imageStore(ocm, ivec2(gl_FragCoord.xy), vec4(1.0, 0, 0, 0.0));
    }
  }
)");


/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Point shaders ///////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

static const std::string g_pointVertexShader = SHADERR( R"(
  in vec3 xyz; 
  uniform mat4 viewMat;
  uniform mat4 projMat;
  void main() { gl_Position = projMat * viewMat * vec4(xyz, 1.0); }
)" );

static const std::string g_pointFragmentShader = SHADERR( R"(
  out vec4 frag_Color;
  uniform vec4 color;
  void main() { frag_Color = color; }
)"; );

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Axe shaders //////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

static const std::string g_axesVertexShader = SHADERR( R"(
  in vec3 xyz; 
  in vec4 color; 
  uniform mat4 viewMat;
  uniform mat4 projMat;
  out vec4 vertex_color; 
  void main() { 
    gl_Position = projMat * viewMat * vec4(xyz, 1.0); 
    vertex_color = color; 
  }
)" );

static const std::string g_axesFragmentShader = SHADERR( R"(
  in vec4  vertex_color;
  out vec4 frag_Color;
  void main() { frag_Color = vertex_color; }
)"; );

// clang-format on
