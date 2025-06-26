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

#include <glad/glad.h>

class Shader {
 public:
  Shader( const std::string& eCode, GLenum eShaderType ) : object_( 0 ) {
    GLint iStatus, iLength = 0;
    code_             = eCode;
    const char* pCode = eCode.c_str();
    object_           = glCreateShader( eShaderType );
    if ( object_ == 0 ) { throw std::runtime_error( "glCreateShader failed" ); }
    glShaderSource( object_, 1, static_cast<const GLchar**>( &pCode ), nullptr );
    glCompileShader( object_ );
    glGetShaderiv( object_, GL_COMPILE_STATUS, &iStatus );
    if ( iStatus == GL_FALSE ) {
      glGetShaderiv( object_, GL_INFO_LOG_LENGTH, &iLength );
      std::vector<char> pString;
      pString.resize( iLength + 1 );
      glGetShaderInfoLog( object_, iLength, nullptr, pString.data() );
      glDeleteShader( object_ );
      object_              = 0;
      std::string eMessage = "Compile failure in shader: \n" + formatCode( eCode ) + "\n" + pString.data();
      printf( "%s \n", eMessage.c_str() );
      fflush( stdout );
      throw std::runtime_error( eMessage );
    }
  }

  ~Shader() = default;
  GLuint             getObject() const { return object_; }
  const std::string& getCode() const { return code_; }

 private:
  GLuint      object_;
  std::string code_;
};

#include <glad/glad.h>
/*! \class %Program
 * \brief %Program class.
 *
 *  This class is used to define a OpenGL shader program.
 */
class Program {
 public:
  /**
   * \brief Constructor.
   **/
  Program() {}

  /**
   * \brief Constructor.
   * \param  eShader %Shader object.
   **/
  Program( const std::vector<Shader>& eShader ) { loadShader( eShader ); }

  /**
   * \brief Constructor.
   * \param  eName Name of the program.
   * \param  eVertexShader Source code of the vertex shader.
   * \param  eFragmentShader Source code of the fragment shader.
   */
  Program( std::string eName, const std::string& eVertexShader, const std::string& eFragmentShader ) :
      object_( 0 ), name_( std::move( eName ) ) {
    create( eName, eVertexShader, eFragmentShader );
  }
  /**
   * \brief Constructor.vector
   * \param  eName Name of the program.
   * \param  eVertexShader Source code of the vertex shader.
   * \param  eGeometryShader Source code of the geometry shader.
   * \param  eFragmentShader Source code of the fragment shader.
   */
  Program( std::string        eName,
           const std::string& eVertexShader,
           const std::string& eGeometryShader,
           const std::string& eFragmentShader ) :
      object_( 0 ), name_( std::move( eName ) ) {
    create( eName, eVertexShader, eGeometryShader, eFragmentShader );
  }
  //! \brief Desctructor.
  ~Program() {
    if ( object_ != 0 ) {
      glUseProgram( 0 );
      glDeleteProgram( object_ );
      object_ = 0;
    }
  }
  /**
   * \brief Create program.
   * \param  eName Name of the program.
   * \param  eComputeShader Source code of the compute shader.
   */
  void create( std::string eName, const std::string& eComputeShader ) {
    name_ = eName;
    std::vector<Shader> eShaders;
    eShaders.emplace_back( eComputeShader, GL_COMPUTE_SHADER );
    loadShader( eShaders );
  }

  /**
   * \brief Create program.
   * \param  eName Name of the program.
   * \param  eVertexShader Source code of the vertex shader.
   * \param  eFragmentShader Source code of the fragment shader.
   */
  void create( std::string eName, const std::string& eVertexShader, const std::string& eFragmentShader ) {
    name_ = eName;
    std::vector<Shader> eShaders;
    eShaders.emplace_back( eVertexShader, GL_VERTEX_SHADER );
    eShaders.emplace_back( eFragmentShader, GL_FRAGMENT_SHADER );
    loadShader( eShaders );
  }
  /**
   * \brief Create program.
   * \param  eName Name of the program.
   * \param  eVertexShader Source code of the vertex shader.
   * \param  eGeometryShader Source code of the geometry shader.
   * \param  eFragmentShader Source code of the fragment shader.
   */
  void create( std::string        eName,
               const std::string& eVertexShader,
               const std::string& eGeometryShader,
               const std::string& eFragmentShader ) {
    name_ = eName;
    std::vector<Shader> eShaders;
    eShaders.emplace_back( eVertexShader, GL_VERTEX_SHADER );
    eShaders.emplace_back( eGeometryShader, GL_GEOMETRY_SHADER );
    eShaders.emplace_back( eFragmentShader, GL_FRAGMENT_SHADER );
    loadShader( eShaders );
  }

  //! \brief Installs a program object as part of current rendering state.
  void use() const { glUseProgram( object_ ); }
  //! \brief Uninstalls a program object as part of current rendering state.
  void stop() const { glUseProgram( 0 ); }

  //! \brief Returns the location of an attribute variable. \param pAttribName Name of the attribute. \return Location
  //! of an attribute variable.
  GLint attrib( const GLchar* pName ) const {
    if ( !pName ) { throw std::runtime_error( "attribName was NULL" ); }
    GLint iAttrib = glGetAttribLocation( object_, pName );
    if ( iAttrib == -1 ) { throw std::runtime_error( std::string( "Program attribute not found: " ) + pName ); }
    return iAttrib;
  }
  //! \brief Returns the location of an uniform variable. \param pUniformName Name of the uniform. \return Location of
  //! an uniform variable.
  GLint uniform( const GLchar* pName ) const {
    if ( !pName ) {
      printf( "Error: uniform name was NULL \n" );
      fflush( stdout );
      exit( -1 );
    }
    GLint iUniform = glGetUniformLocation( object_, pName );
    if ( iUniform == -1 ) {
      printf( "Error: uniform not found: %s \n", pName );
      fflush( stdout );
      exit( -1 );
    }
    return iUniform;
  }

  //! Return the name of the program. \return Name of the program.
  const std::string& getName() { return name_; }

  inline bool isLoad() { return load_; }
  /**
   * \brief Specify the value of a matrix4x4 uniform variable.
   * \param pName Name of the uniform variable.
   * \param eMatrix Value of the Matrix.
   * \param bTranspose Transpose the matrix.
   **/
  void setUniform( const GLchar* pName, const glm::mat4& eMatrix, GLboolean bTranspose = GL_FALSE ) {
    glUniformMatrix4fv( uniform( pName ), 1, bTranspose, glm::value_ptr( eMatrix ) );
  }
  /**
   * \brief Specify the value of a vector 3 uniform variable.
   * \param pName Name of the uniform variable.
   * \param eVector Value of the vector.
   **/
  void setUniform( const GLchar* pName, const glm::vec2& eVector ) {
    glUniform2fv( uniform( pName ), 1, glm::value_ptr( eVector ) );
  }
  /**
   * \brief Specify the value of a vector 3 uniform variable.
   * \param pName Name of the uniform variable.
   * \param eVector Value of the vector.
   **/
  void setUniform( const GLchar* pName, const glm::vec3& eVector ) {
    glUniform3fv( uniform( pName ), 1, glm::value_ptr( eVector ) );
  }
  /**
   * \brief Specify the value of a vector 4 uniform variable.
   * \param pName Name of the uniform variable.
   * \param eVector Value of the vector.
   **/
  void setUniform( const GLchar* pName, const glm::vec4& eVector ) {
    glUniform4fv( uniform( pName ), 1, glm::value_ptr( eVector ) );
  }
  /**
   * \brief Specify the value of a float uniform variable.
   * \param pName Name of the uniform variable.
   * \param fValue Value of the variable.
   **/
  void setUniform( const GLchar* pName, const float fValue ) { glUniform1f( uniform( pName ), fValue ); }
  /**
   * \brief Specify the value of a int uniform variable.
   * \param pName Name of the uniform variable.
   * \param iValue Value of the variable.
   **/
  void setUniform( const GLchar* pName, const int iValue ) { glUniform1i( uniform( pName ), iValue ); }
  /**
   * \brief Specify the value of a int uniform variable.
   * \param pName Name of the uniform variable.
   * \param uValue Value of the variable.
   **/
  void setUniform( const GLchar* pName, const uint32_t uValue ) { glUniform1ui( uniform( pName ), uValue ); }
  /**
   * \brief Specify the value of a int uniform variable.
   * \param pName Name of the uniform variable.
   * \param pVector Value of the variable.
   **/
  void setUniform( const GLchar* pName, const std::vector<float>& pVector ) {
    glUniform1fv( uniform( pName ), static_cast<GLsizei>( pVector.size() ), pVector.data() );
  }
  /**
   * \brief Specify the value of a int uniform variable.
   * \param pName Name of the uniform variable.
   * \param pVector Value of the variable.
   **/
  void setUniform( const GLchar* pName, const std::vector<glm::vec3>& pVector ) {
    glUniform3fv( uniform( pName ), static_cast<GLsizei>( pVector.size() ),
                  reinterpret_cast<const GLfloat*>( pVector.data() ) );
  }

  template <typename T>
  void setLightUniform( const char* eName, size_t iLightIndex, const T& eValue ) {
    std::ostringstream eStringStream;
    eStringStream << "allLights[" << iLightIndex << "]." << eName;
    std::string eString = eStringStream.str();
    setUniform( eString.c_str(), eValue );
  }

 private:
  void loadShader( const std::vector<Shader>& eShaders ) {
    if ( eShaders.empty() ) { throw std::runtime_error( "No eShaders were provided to create the program" ); }
    object_ = glCreateProgram();
    if ( object_ == 0 ) { throw std::runtime_error( "glCreateProgram failed" ); }
    for ( const auto& eShader : eShaders ) { glAttachShader( object_, eShader.getObject() ); }
    glLinkProgram( object_ );
    for ( const auto& eShader : eShaders ) { glDetachShader( object_, eShader.getObject() ); }
    GLint status;
    glGetProgramiv( object_, GL_LINK_STATUS, &status );
    if ( status == GL_FALSE ) {
      std::string pMessage( "Program linking failure: " );
      GLint       iLength;
      glGetProgramiv( object_, GL_INFO_LOG_LENGTH, &iLength );
      std::vector<char> eString;
      eString.resize( iLength + 1 );
      glGetProgramInfoLog( object_, iLength, nullptr, eString.data() );
      for ( const auto& eShader : eShaders ) pMessage += formatCode( eShader.getCode() ) + "\n\n";
      pMessage += eString.data();
      eString.clear();
      glDeleteProgram( object_ );
      object_ = 0;
      printf( "%s \n", pMessage.c_str() );
      fflush( stdout );
      throw std::runtime_error( pMessage );
    }
    load_ = true;
  }

  GLuint      object_ = 0;
  std::string name_   = "";
  bool        load_   = false;
};
