#include "viewpoints.hpp"
#include "program.hpp"
#include "pointcloud.hpp"

class Arrow {
 public:
  Arrow() {}
  ~Arrow() {
    if ( number_ != -1 ) {
      glDeleteBuffers( 1, &vbo_ );
      glDeleteVertexArrays( 1, &vao_ );
      number_ = -1;
    }
  }

  void load( glm::vec3 A, glm::vec3 B, float fA, float fB, glm::vec4 color ) {
    if ( !program_.isLoad() ) {
      static const std::string vertexShader =
          SHADER( layout( location = 0 ) in vec3 vertex_position; uniform mat4 ProjMat; uniform mat4 ModMat;
                  void main() { gl_Position = ProjMat * ModMat * vec4( vertex_position, 1.0 ); } );
      static const std::string fragmentShader =
          SHADER( uniform vec4 color; out vec4 frag_colour; void main() { frag_colour = color; } );
      program_.create( "Color", vertexShader, fragmentShader );
    }
    std::vector<glm::vec3> points;
    points.push_back( A );
    points.push_back( B );
    glm::vec3 X = B - A, Z, Y;
    X *= fA / glm::length( X );
    glm::vec3 V0( fB * ( -X[1] ) / fA, fB * ( X[0] ) / fA, 0 );
    glm::vec3 V1( fB * ( -X[2] ) / fA, 0, fB * ( X[0] ) / fA );
    glm::vec3 V2( 0, fB * ( -X[2] ) / fA, fB * ( X[1] ) / fA );
    float     fNormZ0 = glm::dot( V0, V0 ), fNormZ1 = glm::dot( V1, V1 ), fNormZ2 = glm::dot( V2, V2 );
    if ( fNormZ1 > fNormZ2 ) {
      Z = V1;
      Y = fNormZ0 > fNormZ2 ? V0 : V2;
    } else {
      Z = V2;
      Y = fNormZ0 > fNormZ1 ? V0 : V1;
    }
    points.push_back( B );
    points.push_back( B - X + Y - Z );
    points.push_back( B - X + Y + Z );
    points.push_back( B - X - Y + Z );
    points.push_back( B );
    points.push_back( B - X - Y - Z );
    points.push_back( B - X + Y - Z );
    points.push_back( B - X - Y + Z );
    glGenBuffers( 1, &vbo_ );
    glBindBuffer( GL_ARRAY_BUFFER, vbo_ );
    glBufferData( GL_ARRAY_BUFFER, points.size() * sizeof( glm::vec3 ), points.data(), GL_STATIC_DRAW );
    glGenVertexArrays( 1, &vao_ );
    glBindVertexArray( vao_ );
    glBindBuffer( GL_ARRAY_BUFFER, vbo_ );
    glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, NULL );
    glEnableVertexAttribArray( 0 );
    number_ = (int)points.size();
    color_  = color;
  }

  void draw( glm::mat4& eMatMod, glm::mat4& eMatPro ) {
    program_.use();
    program_.setUniform( "ModMat", eMatMod );
    program_.setUniform( "ProjMat", eMatPro );
    program_.setUniform( "color", color_ );
    glBindVertexArray( vao_ );
    glDrawArrays( GL_TRIANGLE_STRIP, 0, number_ );
    glDrawArrays( GL_LINES, 0, number_ );
    glBindVertexArray( 0 );
    program_.stop();
  }

 private:
  GLuint    vao_ = 0;
  GLuint    vbo_ = 0;
  Program   program_;
  int       number_ = -1;
  glm::vec4 color_;
};

// Splat Vertex Shader
static const std::string g_splatVertexShader = SHADERR( R"(
  uniform mat4 viewMat;  
  uniform mat4 projMat;  
  uniform vec4 window;   
  uniform vec3 eye;
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
    return vec3(dot(sh00, b0) + dot(sh01, b1) + dot(sh02, b2) + dot(sh03, b3) + 0.5,
                dot(sh10, b0) + dot(sh11, b1) + dot(sh12, b2) + dot(sh13, b3) + 0.5,
                dot(sh20, b0) + dot(sh21, b1) + dot(sh22, b2) + dot(sh23, b3) + 0.5);
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
    geom_color  = vec4(computeRadiance(normalize(xyz - eye)), 1.0 / (1.0 + exp(-opacity)));
    gl_Position = p;
  }
  )" );

// Splat geometry shader
static const std::string g_splatGeometryShader = SHADERR( R"(
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
  )" );

// Splat fragment shader
static const std::string g_splatFragmentShader = SHADERR( R"(
  in vec4 frag_color;  
  in vec4 frag_cov; 
  in vec2 frag_screen;  
  out vec4 out_color;
  void main() {
    vec2    d = gl_FragCoord.xy - frag_screen;    
    float   g = exp(-0.5f * dot(d, mat2(frag_cov.xy, frag_cov.zw) * d));    
    out_color = vec4( frag_color.a * g * frag_color.rgb, frag_color.a * g );
    if ((frag_color.a * g) <= (1.0f / 256.0f))
      discard;
  }
  )" );

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

void Viewpoints::mouseCallback( GLFWwindow* window, double xpos, double ypos ) {
  if ( instance_ ) {
    int width = 0, height = 0;
    glfwGetWindowSize( window, &width, &height );
    const int r = width < height ? width : height;
    xpos        = ( 2.0 * xpos - width ) / r;
    ypos        = ( height - 2.0 * ypos ) / r;
    if ( glfwGetMouseButton( window, GLFW_MOUSE_BUTTON_LEFT ) == GLFW_PRESS ) {
      if ( instance_->firstMouse_ ) {
        instance_->lastX_      = xpos;
        instance_->lastY_      = ypos;
        instance_->firstMouse_ = false;
      }
      instance_->camera_.rotate( instance_->lastX_, instance_->lastY_, xpos, ypos );
    } else if ( glfwGetMouseButton( window, GLFW_MOUSE_BUTTON_RIGHT ) == GLFW_PRESS ) {
      if ( instance_->firstMouse_ ) {
        instance_->lastX_      = xpos;
        instance_->lastY_      = ypos;
        instance_->firstMouse_ = false;
      }
      instance_->camera_.translate( ( instance_->lastX_ - xpos ) * ( width / 50 ),   //
                                    ( instance_->lastY_ - ypos ) * ( height / 50 ),  //
                                    0.f );
    } else {
      instance_->firstMouse_ = true;
    }
    instance_->lastX_ = xpos;
    instance_->lastY_ = ypos;
  }
}

void Viewpoints::scrollCallback( GLFWwindow* window, double xoffset, double yoffset ) {
  if ( instance_ ) { instance_->camera_.zoom( static_cast<float>( yoffset * 10 ) ); }
}

void Viewpoints::keyCallback( GLFWwindow* window, int key, int scancode, int action, int mods ) {
  if ( instance_ ) {
    if ( action == GLFW_PRESS || action == GLFW_REPEAT ) {
      switch ( key ) {
        case GLFW_KEY_T: {
          auto& el = instance_->viewpoints_[instance_->viewIndex_];
          instance_->camera_.setLookAt( el.pos(), el.view(), el.up() );
          instance_->viewIndex_ = ( instance_->viewIndex_ + 1 ) % instance_->viewpoints_.size();
        } break;
        case GLFW_KEY_D: {
          std::tuple<glm::vec3, glm::vec3, glm::vec3> value = instance_->camera_.getLookAt();
          printf( "%12.6f %12.6f %12.6f     %12.6f %12.6f %12.6f      %12.6f %12.6f %12.6f \n",  //
                  std::get<0>( value ).x, std::get<0>( value ).y, std::get<0>( value ).z,        //
                  std::get<1>( value ).x, std::get<1>( value ).y, std::get<1>( value ).z,        //
                  std::get<2>( value ).x, std::get<2>( value ).y, std::get<2>( value ).z );
          fflush( stdout );
        } break;
        default: break;
      }
    }
  }
}

glm::vec4 getRandomColor() {
  float r = static_cast<float>( rand() ) / static_cast<float>( RAND_MAX );
  float g = static_cast<float>( rand() ) / static_cast<float>( RAND_MAX );
  float b = static_cast<float>( rand() ) / static_cast<float>( RAND_MAX );
  float a = 1.0f;
  return glm::vec4( r, g, b, a );
}

void setBuffer2( GLuint uiBuffer, const std::vector<uint32_t>& vec, size_t size ) {
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

void Viewpoints::display( const Pointcloud& pc, int width, int height ) {
  if ( !glfwInit() ) {
    std::cerr << "Erreur : impossible d'initialiser GLFW\n";
    return;
  }
  glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, SHADER_VERSION_MAJOR );
  glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, SHADER_VERSION_MINOR );
  glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );
#if defined( __APPLE__ )
  glfwWindowHint( GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE );
#endif
  auto window = glfwCreateWindow( width, height, "Points and Viewpoints Display", nullptr, nullptr );
  if ( !window ) {
    std::cerr << "Erreur : impossible de créer la fenêtre GLFW\n";
    glfwTerminate();
    return;
  }
  glfwMakeContextCurrent( window );
  if ( !gladLoadGLLoader( (GLADloadproc)glfwGetProcAddress ) ) {
    std::cerr << "Erreur : impossible d'initialiser GLAD\n";
    return;
  }
  Camera camera;
  camera.create( 10.f, 45.f );
  glfwSetCursorPosCallback( window, Viewpoints::mouseCallback );
  glfwSetScrollCallback( window, Viewpoints::scrollCallback );
  glfwSetKeyCallback( window, Viewpoints::keyCallback );

  Program programPoints;
  programPoints.create( "Splat", g_splatVertexShader, g_splatGeometryShader, g_splatFragmentShader );
  programPoints.use();

  // Create VAO
  GLuint vao;
  GLuint vbo;
  GLuint vboIndex;
  glGenVertexArrays( 1, &vao );
  glBindVertexArray( vao );
  const uint32_t stride = (uint32_t)sizeof( GaussianData );
  glGenBuffers( 1, &vbo );
  glBindBuffer( GL_ARRAY_BUFFER, vbo );
  glBufferData( GL_ARRAY_BUFFER, stride * pc.size(), pc.data(), GL_STATIC_DRAW );

  // Create VBO for index data
  int32_t               numPoints = pc.size();
  std::vector<uint32_t> depth( numPoints );
  std::vector<uint32_t> index( numPoints );
  glGenBuffers( 1, &vboIndex );
  glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, vboIndex );
  glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof( uint32_t ) * index.size(), (void*)index.data(), GL_DYNAMIC_DRAW );

  // Set shader attributes
  glVertexAttribPointer( programPoints.attrib( "xyz" ), 3, GL_FLOAT, GL_FALSE, stride,
                         (void*)offsetof( GaussianData, xyz_ ) );
  glEnableVertexAttribArray( programPoints.attrib( "xyz" ) );
  glVertexAttribPointer( programPoints.attrib( "opacity" ), 1, GL_FLOAT, GL_FALSE, stride,
                         (void*)offsetof( GaussianData, opacity_ ) );
  glEnableVertexAttribArray( programPoints.attrib( "opacity" ) );
  for ( int c = 0; c < 3; c++ )
    for ( int i = 0; i < 4; ++i ) {
      auto attribute = programPoints.attrib( ( "sh" + std::to_string( c ) + std::to_string( i ) ).c_str() );
      glVertexAttribPointer( attribute, 4, GL_FLOAT, GL_FALSE, stride,
                             (void*)( offsetof( GaussianData, sh_ ) + ( c * 4 + i ) * sizeof( glm::vec4 ) ) );
      glEnableVertexAttribArray( attribute );
    }
  glVertexAttribPointer( programPoints.attrib( "scale" ), 3, GL_FLOAT, GL_FALSE, stride,
                         (void*)offsetof( GaussianData, scale_ ) );
  glEnableVertexAttribArray( programPoints.attrib( "scale" ) );
  glVertexAttribPointer( programPoints.attrib( "rotate" ), 4, GL_FLOAT, GL_FALSE, stride,
                         (void*)offsetof( GaussianData, rotate_ ) );
  glEnableVertexAttribArray( programPoints.attrib( "rotate" ) );

  programPoints.stop();

  glEnable( GL_BLEND );
  glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
  glEnable( GL_DEPTH_TEST );
  glDepthMask( GL_TRUE );

  glLineWidth( 2 );

  std::vector<Arrow> axes( 3 );
  axes[0].load( glm::vec3( 0, 0, 0 ), glm::vec3( 1, 0, 0 ), 0.1, 0.05, glm::vec4( 1., 0., 0., 1. ) );
  axes[1].load( glm::vec3( 0, 0, 0 ), glm::vec3( 0, 1, 0 ), 0.1, 0.05, glm::vec4( 0., 1., 0., 1. ) );
  axes[2].load( glm::vec3( 0, 0, 0 ), glm::vec3( 0, 0, 1 ), 0.1, 0.05, glm::vec4( 0., 0., 1., 1. ) );

  std::vector<Arrow> arrows( viewpoints_.size() );
  for ( size_t i = 0; i < viewpoints_.size(); i++ )
    arrows[i].load( viewpoints_[i].pos() + glm::normalize( viewpoints_[i].pos() - viewpoints_[i].view() ),
                    viewpoints_[i].pos(), 0.1, 0.05, getRandomColor() );

  camera_.initialize( width, height );
  camera_.setLookAt( glm::vec3( -15, 0, -2 ), glm::vec3( 0, 0.5, 0 ), glm::vec3( 0, -1, 0 ) );

  const uint32_t maxValue = std::numeric_limits<uint32_t>::max();
  while ( !glfwWindowShouldClose( window ) ) {
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    glClearColor( 0.f, 0.f, 0.f, 1.f );
    glm::mat4 projMat = camera_.projMat();
    glm::mat4 viewMat = camera_.viewMat();
    auto      pos     = camera_.center();
    auto      nearFar = camera_.nearFar();
    glBindVertexArray( vao );
    int32_t count         = 0;
    auto    modelViewProj = projMat * viewMat;
    for ( int32_t i = 0; i < numPoints; ++i ) {
      glm::vec4 p = modelViewProj * glm::vec4( pc[i].xyz_, 1.f );
      if ( p.w > nearFar.x && p.w < nearFar.y && std::abs( p.x / p.w ) < 1.2 && std::abs( p.y / p.w ) < 1.2 ) {
        depth[count] = uint32_t( (float)maxValue * ( nearFar.y - p.w ) / ( nearFar.y - nearFar.x ) );
        index[count] = i;
        count++;
      }
    }
    std::vector<uint32_t> indices( count );
    std::iota( indices.begin(), indices.end(), 0 );
    std::sort( indices.begin(), indices.end(), [&]( uint32_t a, uint32_t b ) { return depth[a] < depth[b]; } );
    std::vector<uint32_t> sorted( count );
    std::transform( indices.begin(), indices.end(), sorted.begin(), [&]( uint32_t idx ) { return index[idx]; } );
    setBuffer2( vboIndex, sorted, count );

    programPoints.use();
    programPoints.setUniform( "viewMat", viewMat );
    programPoints.setUniform( "projMat", projMat );
    programPoints.setUniform( "eye", pos );
    programPoints.setUniform( "window", glm::vec4( (float)width, (float)height, nearFar ) );
    glEnableVertexAttribArray( 0 );
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, vboIndex );
    glDrawElements( GL_POINTS, count, GL_UNSIGNED_INT, nullptr );
    glDisableVertexAttribArray( 0 );
    glBindVertexArray( 0 );
    programPoints.stop();

    glDisable( GL_DEPTH_TEST );
    glDepthMask( GL_FALSE );
    for ( auto& el : axes ) el.draw( viewMat, projMat );
    for ( auto& el : arrows ) el.draw( viewMat, projMat );

    glfwSwapBuffers( window );
    glfwPollEvents();
  }
  glDeleteBuffers( 1, &vbo );
  glDeleteVertexArrays( 1, &vao );
  glfwDestroyWindow( window );
  glfwTerminate();
}

Viewpoints* Viewpoints::instance_ = nullptr;