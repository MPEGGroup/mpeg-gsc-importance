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
#include "rasterizerHardware.hpp"
#include "pointcloud.hpp"
#include "image.hpp"
#include "viewpoints.hpp"

// clang-format off

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Splat shader ////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

// Splat Vertex Shader
static const std::string g_splatVertexShader = SHADERR(  R"(
  precision highp float;
  uniform mat4 viewMat;  
  uniform mat4 projMat;  
  uniform vec4 window;   
  uniform vec3 pos;
  in vec3  xyz; 
  in float opacity;
  in vec4  sh00, sh10, sh20, sh01, sh11, sh21, sh02, sh12, sh22, sh03, sh13, sh23;
  in vec3  scale; 
  in vec4  rotate; 
  out vec4 geom_color;   
  out vec4 geom_cov;    
  out vec2 geom_screen;      
  const float k[10] = float[]( 0.28209479177387814, 0.4886025119029199, 1.0925484305920792, 0.31539156525252005,  
                               0.5462742152960396,  0.5900435899266435, 2.8906114426405543, 0.4570457994644658, 
                               0.37317633259011546, 1.4453057213202771 );
  mat3 quatToMat(vec4 q) {    
		return mat3(1 - 2 * (q.y * q.y + q.z * q.z),     2 * (q.x * q.y + q.w * q.z),     2 * (q.x * q.z - q.w * q.y),
                    2 * (q.x * q.y - q.w * q.z), 1 - 2 * (q.x * q.x + q.z * q.z),     2 * (q.y * q.z + q.w * q.x),
                    2 * (q.x * q.z + q.w * q.y),     2 * (q.y * q.z - q.w * q.x), 1 - 2 * (q.x * q.x + q.y * q.y));
	}
  vec3 computeRadiance(const vec3 direction) {
    vec3 v = normalize(direction); 
    vec3 v2 = v * v;
    vec4 b0 = vec4( k[0], -k[1] * v.y, k[1] * v.z, -k[1] * v.x );
    vec4 b1 = vec4( k[2] * v.y * v.x, -k[2] * v.y * v.z, k[3] * ( 3 * v2.z - 1 ), -k[2] * v.x * v.z);
    vec4 b2 = vec4( k[4] * ( v2.x - v2.y ),  -k[5] * v.y * ( 3 * v2.x - v2.y), 
                    k[6] * v.y * v.x * v.z,  -k[7] * v.y * ( 5 * v2.z - 1) );
    vec4 b3 = vec4( k[8] * v.z * ( 5 * v2.z - 3 ), -k[7] * v.x * ( 5 * v2.z - 1 ), 
                    k[9] * v.z * ( v2.x - v2.y ),  -k[5] * v.x * ( v2.x - 3 * v2.y ) );
    vec3 result = vec3(dot(sh00, b0) + dot(sh01, b1) + dot(sh02, b2) + dot(sh03, b3) + 0.5,
                dot(sh10, b0) + dot(sh11, b1) + dot(sh12, b2) + dot(sh13, b3) + 0.5,
                dot(sh20, b0) + dot(sh21, b1) + dot(sh22, b2) + dot(sh23, b3) + 0.5);
                
    // return direction + result * 0.0000000001;
    return result;
  }
  void main(void) {
    vec4 t      = viewMat * vec4(xyz, 1.0);
    vec4 p      = projMat * t;
    mat3 J      = mat3( vec3(-projMat[0][0] * window.x / (2.0 * t.z), 0.0,  projMat[0][0] * window.x * t.x / (2.0 *  t.z * t.z)),
                        vec3(0.0, -projMat[1][1] * window.y / (2.0 * t.z), projMat[1][1] * window.y * t.y / (2.0 *  t.z * t.z)),
                        vec3(0.0, 0.0, (window.w - window.z) * projMat[3][2] / (2.0 *  t.z * t.z)) );
    mat3 R      = quatToMat(normalize(rotate));
    mat3 S      = mat3( vec3(exp(scale.x), 0.0, 0.0), vec3(0.0, exp(scale.y), 0.0), vec3(0.0, 0.0, exp(scale.z)) );
    mat3 C      = R * S * transpose(S) * transpose(R);    
    mat3 JW     = J * mat3(viewMat);
    mat3 V      = JW * C * transpose(JW);
    geom_cov    = vec4(V[0][0], V[0][1], V[1][0], V[1][1]) + vec4(0.6, 0.0, 0.0, 0.6); // add anti-aliasing low-pass filter
    geom_screen = 0.5 * window.xy * (p.xy / p.w + vec2(1.0));
    geom_color  = vec4(computeRadiance(xyz - pos), 1.0 / (1.0 + exp(-opacity)));     
    gl_Position = p;
  }
  )"
);

// Splat geometry shader
static const std::string g_splatGeometryShader = SHADERR(  R"(
  precision highp float;
  uniform vec4 window;  
  layout(points) in;
  layout(triangle_strip, max_vertices = 4) out;
  in vec4  geom_color[];  
  in vec4  geom_cov[];  
  in vec2  geom_screen[];  
  out vec4 frag_color;  
  out vec4 frag_cov;  
  out vec2 frag_screen;  
  vec4 invCov(vec4 v) { return vec4(v.w, -v.y, -v.z, v.x) / ( v.x * v.w - v.y * v.z ); }
  void main() {
    vec3 ndc = gl_in[0].gl_Position.xyz / gl_in[0].gl_Position.w;
    if (any(greaterThan(abs(ndc), vec3(1.2, 1.2, 1.0))) || ndc.z < 0.25f) { return; }
    float avg   = (geom_cov[0].x + geom_cov[0].w) * 0.5;  
    float diff  = (geom_cov[0].x - geom_cov[0].w) * 0.5;  
    float diag  = sqrt(diff * diff + geom_cov[0].y * geom_cov[0].y);  
    float angle = (geom_cov[0].y == 0.0) ? (geom_cov[0].x >= geom_cov[0].w ? 0.0 : radians(90.0)) : 
                                           atan(avg + diag - geom_cov[0].x, geom_cov[0].y);
    vec2 scale  = vec2(2.0 / window.x, 2.0 / window.y) * gl_in[0].gl_Position.w;
    vec2 a0     = 3.5f * sqrt(avg + diag) * vec2( cos(angle), sin(angle));
    vec2 a1     = 3.5f * sqrt(avg - diag) * vec2(-sin(angle), cos(angle));
    frag_cov    = invCov( geom_cov[0] );
    frag_color  = geom_color[0];
    frag_screen = geom_screen[0];
    vec2 off[4] = vec2[4]( a0 + a1, -a0 + a1,  a0 - a1, -a0 - a1 );
    for (int i = 0; i < 4; i++) { 
      gl_Position = gl_in[0].gl_Position + vec4(off[i] * scale, 0.0, 0.0); 
      EmitVertex(); 
    }
    EndPrimitive();
  }
  )"
);

// Splat fragment shader
static const std::string g_splatFragmentShader = SHADERR(  R"(
  precision highp float;
  in vec4 frag_color;  
  in vec4 frag_cov; 
  in vec2 frag_screen;  
  out vec4 out_color;
  void main() {
    vec2    d = gl_FragCoord.xy - frag_screen;    
    float   g = exp(-0.5f * dot(d, mat2(frag_cov.xy, frag_cov.zw) * d));    
    if ((frag_color.a * g) <= (1.0f / 256.0f))
       discard;
    out_color = vec4( frag_color.a * g * frag_color.rgb, frag_color.a * g );
    // out_color = frag_color; 
  }
  )"
);

// clang-format on

RasterizerHardware::RasterizerHardware() {}

RasterizerHardware::~RasterizerHardware() {
  unload();
  reset();
}

static void error_callback( int error, const char* description ) {
  std::cerr << "Error: " << description << std::endl;
  ;
}

bool RasterizerHardware::initialize() {
  reset();
  glfwSetErrorCallback( error_callback );

  if ( !glfwInit() ) return false;
  glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, SHADER_VERSION_MAJOR );
  glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, SHADER_VERSION_MINOR );
  glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE );
  glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
  glfwWindowHint( GLFW_SRGB_CAPABLE, GLFW_TRUE );
  glfwWindowHint( GLFW_VISIBLE, GLFW_FALSE );
  window_ = glfwCreateWindow( 64, 64, "Window", nullptr, nullptr );
  if ( !window_ ) {
    printf( "Systen can't initilialize glfw windows \n" );
    fflush( stdout );
    glfwTerminate();
    return false;
  }
  glfwMakeContextCurrent( window_ );

  if ( !gladLoadGLLoader( (GLADloadproc)glfwGetProcAddress ) ) {
    printf( "Systen can't initilialize GLAD \n" );
    fflush( stdout );
    glfwTerminate();
    return false;
  }

  program_.create( "GPU", g_splatVertexShader, g_splatGeometryShader, g_splatFragmentShader );
  glEnable( GL_BLEND );
  glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
  glEnable( GL_DEPTH_TEST );
  glDepthMask( GL_TRUE );
  glGenFramebuffers( 1, &fbo_ );
  glBindFramebuffer( GL_FRAMEBUFFER, fbo_ );
  glGenRenderbuffers( 1, &fboTexture_ );
  glBindRenderbuffer( GL_RENDERBUFFER, fboTexture_ );
  glRenderbufferStorage( GL_RENDERBUFFER, GL_RGBA, width_, height_ );
  glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, fboTexture_ );
  GLenum status;
  status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
  if ( status != GL_FRAMEBUFFER_COMPLETE ) {
    printf( "Error: FBO not complete \n" );
    fflush( stdout );
    glfwTerminate();
    return false;
  }
  return true;
}

void RasterizerHardware::reset() {
  if ( window_ ) {
    glDeleteFramebuffers( 1, &fbo_ );
    glDeleteRenderbuffers( 1, &fboTexture_ );
    glfwDestroyWindow( window_ );
    glfwTerminate();
    window_ = nullptr;
  }
}

void RasterizerHardware::load( const Pointcloud& pc ) {
  unload();

  // Create VAO
  glGenVertexArrays( 1, &vao_ );
  glBindVertexArray( vao_ );

  const uint32_t stride = (uint32_t)sizeof( GaussianData );

  // Create VBO for gaussian data
  glGenBuffers( 1, &vboGaussianData_ );
  glBindBuffer( GL_ARRAY_BUFFER, vboGaussianData_ );
  glBufferData( GL_ARRAY_BUFFER, stride * pc.size(), pc.data(), GL_STATIC_DRAW );

  // Create VBO for index data
  std::vector<uint32_t> index( pc.size() );
  glGenBuffers( 1, &vboIndex_ );
  glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, vboIndex_ );
  glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof( uint32_t ) * index.size(), (void*)index.data(), GL_DYNAMIC_DRAW );

  // Set shader attributes
  glVertexAttribPointer( program_.attrib( "xyz" ), 3, GL_FLOAT, GL_FALSE, stride,
                         (void*)offsetof( GaussianData, xyz_ ) );
  glEnableVertexAttribArray( program_.attrib( "xyz" ) );
  glVertexAttribPointer( program_.attrib( "opacity" ), 1, GL_FLOAT, GL_FALSE, stride,
                         (void*)offsetof( GaussianData, opacity_ ) );
  glEnableVertexAttribArray( program_.attrib( "opacity" ) );
  for ( int c = 0; c < 3; c++ )
    for ( int i = 0; i < 4; ++i ) {
      auto attribute = program_.attrib( ( "sh" + std::to_string( c ) + std::to_string( i ) ).c_str() );
      glVertexAttribPointer( attribute, 4, GL_FLOAT, GL_FALSE, stride,
                             (void*)( offsetof( GaussianData, sh_ ) + ( c * 4 + i ) * sizeof( glm::vec4 ) ) );
      glEnableVertexAttribArray( attribute );
    }
  glVertexAttribPointer( program_.attrib( "scale" ), 3, GL_FLOAT, GL_FALSE, stride,
                         (void*)offsetof( GaussianData, scale_ ) );
  glEnableVertexAttribArray( program_.attrib( "scale" ) );
  glVertexAttribPointer( program_.attrib( "rotate" ), 4, GL_FLOAT, GL_FALSE, stride,
                         (void*)offsetof( GaussianData, rotate_ ) );
  glEnableVertexAttribArray( program_.attrib( "rotate" ) );

  // Disable VAO and VBO
  glBindBuffer( GL_ARRAY_BUFFER, 0 );
  glBindBuffer( GL_SHADER_STORAGE_BUFFER, 0 );
  glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
  glBindBuffer( GL_ATOMIC_COUNTER_BUFFER, 0 );
  glBindVertexArray( 0 );

  loaded_ = true;
}

void RasterizerHardware::unload() {
  if ( loaded_ ) {
    glDeleteBuffers( 1, &vboGaussianData_ );
    glDeleteBuffers( 1, &vboIndex_ );
    glDeleteVertexArrays( 1, &vao_ );
    loaded_ = false;
  }
}

void getBuffer( GLuint uiBuffer, std::vector<uint32_t>& vec, size_t size ) {
  GLenum error = glGetError();
  if ( error != GL_NO_ERROR ) {
    printf( "OpenGL error code: 0x%x before getBuffer\n", error );
    fflush( stdout );
  }
  glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, uiBuffer );
  void* rawKey = glMapBufferRange( GL_ELEMENT_ARRAY_BUFFER, 0, sizeof( uint32_t ) * size, GL_MAP_READ_BIT );
  if ( !rawKey ) {
    printf( "Error: glMapBufferRange failed, OpenGL error: 0x%x\n", glGetError() );
    fflush( stdout );
  } else {
    if ( vec.size() < size ) { vec.resize( size ); }
    memcpy( vec.data(), rawKey, sizeof( uint32_t ) * size );
  }
  if ( !glUnmapBuffer( GL_ELEMENT_ARRAY_BUFFER ) ) {
    printf( "Error: glUnmapBuffer failed, OpenGL error: 0x%x\n", glGetError() );
    fflush( stdout );
  }
  error = glGetError();
  if ( error != GL_NO_ERROR ) {
    printf( "OpenGL error code: 0x%x after getBuffer\n", error );
    fflush( stdout );
  }
}

void setBuffer( GLuint uiBuffer, const std::vector<uint32_t>& vec, size_t size ) {
  GLenum error;
  glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, uiBuffer );
  error = glGetError();
  if ( error != GL_NO_ERROR ) {
    printf( "OpenGL error: Failed to bind buffer 0x%x\n", error );
    return;
  }
  glBufferSubData( GL_ELEMENT_ARRAY_BUFFER, 0, size * sizeof( uint32_t ), vec.data() );
  error = glGetError();
  if ( error != GL_NO_ERROR ) {
    printf( "OpenGL error: glBufferSubData failed with code 0x%x\n", error );
    return;
  }
  glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
}

template <typename T, int N>
void RasterizerHardware::render( const Pointcloud& pc, const Viewpoint& viewpoint, Image<T, N>& image ) {
  if ( width_ != image.width() || height_ != image.height() ) {
    width_  = image.width();
    height_ = image.height();
    if ( !initialize() ) {
      printf( "Error during the GLFW window creation \n" );
      fflush( stdout );
      exit( -1 );
    }
  }
  load( pc );

  glBindVertexArray( vao_ );
  glBindFramebuffer( GL_FRAMEBUFFER, fbo_ );
  glViewport( 0, 0, width_, height_ );
  glClearColor( 0.f, 0.f, 0.f, 1.f );
  glClear( GL_COLOR_BUFFER_BIT );

  // Sort
  int32_t               numPoints = pc.size();
  int32_t               count     = 0;
  std::vector<uint32_t> depth( numPoints );
  std::vector<uint32_t> index( numPoints );
  const uint32_t        maxValue = std::numeric_limits<uint32_t>::max();
  Camera                camera( image.width(), image.height(), viewpoint.pos(), viewpoint.view(), viewpoint.up() );
  glm::mat4             viewMat       = camera.viewMat();
  auto                  pos           = camera.center();
  auto                  projMat       = camera.projMat();
  auto                  nearFar       = camera.nearFar();
  auto                  modelViewProj = projMat * viewMat;
  for ( int32_t i = 0; i < numPoints; ++i ) {
    glm::vec4 p = modelViewProj * glm::vec4( pc[i].xyz_, 1.f );
    if ( p.w > nearFar.x && p.w < nearFar.y && std::abs( p.x / p.w ) < 1.2 && std::abs( p.y / p.w ) < 1.2 ) {
      depth[count] = uint32_t( ( (float)maxValue ) * ( nearFar.y - p.w ) / ( nearFar.y - nearFar.x ) );
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
  setBuffer( vboIndex_, sorted, count );

  // Rasterize
  Image<glm::vec4, 1> buffer( width_, height_, glm::vec4( 0 ) );
  program_.use();
  program_.setUniform( "viewMat", viewMat );
  program_.setUniform( "projMat", projMat );
  program_.setUniform( "pos", pos );
  program_.setUniform( "window", glm::vec4( (float)width_, (float)height_, nearFar ) );

  glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, vboIndex_ );
  glDrawElements( GL_POINTS, count, GL_UNSIGNED_INT, nullptr );
  glReadBuffer( GL_COLOR_ATTACHMENT0 );
  glReadPixels( 0, 0, width_, height_, GL_RGBA, GL_FLOAT, buffer[0].data() );

  program_.stop();
  glBindVertexArray( 0 );
  glBindBuffer( GL_ARRAY_BUFFER, 0 );
  glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
  glBindFramebuffer( GL_FRAMEBUFFER, 0 );

  // Convert vec4 image to output image
  const int32_t size = width_ * height_;
  if constexpr ( std::is_same_v<T, uint8_t> ) {
    const float scale = 255.f;
    for ( int i = 0; i < size; ++i ) {
      glm::vec4& current = buffer[0].get( i );
      image[0].get( i )  = (T)roundf( current.r * scale );
      image[1].get( i )  = (T)roundf( current.g * scale );
      image[2].get( i )  = (T)roundf( current.b * scale );
      if ( N == 4 ) image[3].get( i ) = ( T )( current.a * scale );
    }
  } else if constexpr ( std::is_same_v<T, uint16_t> ) {
    const float scale = 65535.f;
    for ( int i = 0; i < size; ++i ) {
      glm::vec4& current = buffer[0].get( i );
      image[0].get( i )  = (T)roundf( current.r * scale );
      image[1].get( i )  = (T)roundf( current.g * scale );
      image[2].get( i )  = (T)roundf( current.b * scale );
      if ( N == 4 ) image[3].get( i ) = ( T )( current.a * scale );
    }
  } else if constexpr ( std::is_same_v<T, float> ) {
    for ( int i = 0; i < size; ++i ) {
      glm::vec4& current = buffer[0].get( i );
      image[0].get( i )  = current.r;
      image[1].get( i )  = current.g;
      image[2].get( i )  = current.b;
      if ( N == 4 ) image[3].get( i ) = ( T )( current.a );
    }
  } else {
    printf( "Error: template type not support in %s\n", __FILE__ );
    fflush( stdout );
    exit( -1 );
  }
}

template void RasterizerHardware::render<uint8_t>( Pointcloud const&, const Viewpoint&, Image<uint8_t>& );
template void RasterizerHardware::render<uint16_t>( Pointcloud const&, const Viewpoint&, Image<uint16_t>& );
template void RasterizerHardware::render<float>( Pointcloud const&, const Viewpoint&, Image<float>& );