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

enum class ImageFormat { PNG, BMP, TGA, JPG };

template <typename T>
struct Plane {
  void zero() { fill( T( 0 ) ); }
  void fill( T v ) { std::fill( buffer_.begin(), buffer_.end(), v ); }
  void resize( int32_t w = 0, int32_t h = 0, T initValue = T( 0 ) ) {
    width_  = w;
    height_ = h;
    buffer_.resize( w * h, initValue );
  }

  template <typename D>
  Plane& operator=( const Plane<D>& src ) {
    resize( src.width(), src.height() );
    for ( size_t i = 0; i < buffer_.size(); i++ ) set( i, (T)src.get( i ) );
    return *this;
  }

  inline int32_t        size() const { return (int32_t)buffer_.size(); }
  inline T              get( int32_t i ) const { return buffer_[i]; }
  inline T&             get( int32_t i ) { return buffer_[i]; }
  inline T              get( int32_t u, int32_t v ) const { return buffer_[v * width_ + u]; }
  inline T&             get( int32_t u, int32_t v ) { return buffer_[v * width_ + u]; }
  inline T              operator()( int32_t u, int32_t v ) const { return buffer_[v * width_ + u]; }
  inline T&             operator()( int32_t u, int32_t v ) { return buffer_[v * width_ + u]; }
  inline T              operator()( int32_t i ) const { return buffer_[i]; }
  inline T&             operator()( int32_t i ) { return buffer_[i]; }
  inline T*             data( int32_t u, int32_t v ) { return buffer_ + ( v * width_ + u ); }
  inline const T*       data( int32_t u, int32_t v ) const { return buffer_.data() + ( v * width_ + u ); }
  inline void           set( int32_t index, T e ) { buffer_[index] = e; }
  inline void           set( int32_t u, int32_t v, T e ) { buffer_[v * width_ + u] = e; }
  T*                    data() { return buffer_.data(); }
  const T*              data() const { return buffer_.data(); }
  std::vector<T>&       buffer() { return buffer_; }
  const std::vector<T>& buffer() const { return buffer_; }
  int32_t               width() const { return width_; }
  int32_t               height() const { return height_; }
  void                  clear() { buffer_.clear(); }

  Plane operator*( T scalar ) const {
    Plane result;
    result.resize( width_, height_ );
    for ( int32_t i = 0; i < size(); i++ ) { result.get( i ) = this->get( i ) * scalar; }
    return result;
  }
  Plane& operator*=( T scalar ) {
    for ( auto& value : buffer_ ) { value *= scalar; }
    return *this;
  }
  void log( const std::string& str, int32_t u0 = 0, int32_t v0 = 0, int32_t number = 6 ) const {
    printf( "%s: %dx%d \n", str.c_str(), width_, height_ );
    printf( "%4s: ", "Comp" );
    for ( int32_t u = u0; u < std::min( width_, u0 + number ); u++ )
      if constexpr ( std::is_floating_point<T>::value )
        printf( "%6d ", u );
      else
        printf( "%4d ", u );
    printf( "\n" );
    fflush( stdout );
    for ( int32_t v = v0; v < std::min( height_, v0 + number ); v++ ) {
      printf( "%4d: ", v );
      for ( int32_t u = u0; u < std::min( width_, u0 + number ); u++ ) {
        if constexpr ( std::is_floating_point<T>::value )
          printf( "%6.2f ", static_cast<float>( get( u, v ) ) );
        else
          printf( "%4d ", static_cast<int32_t>( get( u, v ) ) );
      }
      printf( "\n" );
    }
  }

 private:
  int32_t        width_;
  int32_t        height_;
  std::vector<T> buffer_;
};

template <typename T, int N = 3>
class Image {
 public:
  Image( const Image& ) = default;
  Image( int32_t width = 0, int32_t height = 0, T value = 0 ) { resize( width, height, value ); }
  ~Image() = default;

  Image( const Image& imageA, const Image& imageB, bool butterfly ) {
    if ( imageA.height_ != imageB.height_ ) {
      printf( "Heigth are not the same \n" );
      fflush( stdout );
      exit( -1 );
    }
    resize( butterfly ? imageA.width_ : ( imageA.width_ + imageB.width_ ), imageA.height_ );
    for ( int c = 0; c < N; c++ ) {
      for ( int v = 0; v < height_; v++ ) {
        if ( butterfly ) {
          int width2 = imageA.width_ / 2;
          for ( int u = 0; u < width2; u++ ) planes_[c].set( u, v, imageA.planes_[c].get( u, v ) );
          for ( int u = 0; u < width2; u++ )
            planes_[c].set( u + width2, v, imageB.planes_[c].get( width2 - 1 - u, v ) );
        } else {
          for ( int u = 0; u < imageA.width_; u++ ) planes_[c].set( u, v, imageA.planes_[c].get( u, v ) );
          for ( int u = 0; u < imageB.width_; u++ )
            planes_[c].set( u + imageA.width_, v, imageB.planes_[c].get( u, v ) );
        }
      }
    }
  }

  void resize( int32_t width, int32_t height, T value = 0 ) {
    width_  = width;
    height_ = height;
    planes_.resize( N );
    for ( auto& plane : planes_ ) plane.resize( width_, height_, value );
  };

  inline void zero() {
    for ( auto& p : planes_ ) { p.zero(); }
  }
  inline void fill( T v ) {
    for ( auto& p : planes_ ) { p.fill( v ); }
  }
  inline void clear() {
    for ( auto& plane : planes_ ) { plane.clear(); }
  }

  Image& operator=( const Image& ) = default;

  template <typename D, int M = 3>
  Image& operator=( const Image<D, M>& src ) {
    resize( src.width(), src.height() );
    for ( int32_t i = 0; i < src.planeCount(); i++ ) { planes_[i] = src.plane( i ); }
    return *this;
  }

  void swap( Image<T, N>& src ) {
    std::swap( width_, src.width_ );
    std::swap( height_, src.height_ );
    std::swap( planes_, src.planes_ );
  }

  Plane<T>&       plane( int32_t index ) { return planes_[index]; }
  const Plane<T>& plane( int32_t index ) const { return planes_[index]; }
  auto            begin() { return planes_.begin(); }
  auto            end() { return planes_.end(); }
  auto            begin() const { return planes_.cbegin(); }
  auto            end() const { return planes_.cend(); }
  Plane<T>&       operator[]( int32_t index ) { return planes_[index]; }
  const Plane<T>& operator[]( int32_t index ) const { return planes_[index]; }
  int32_t         planeCount() const { return int32_t( planes_.size() ); }
  int32_t         width() const { return width_; }
  int32_t         height() const { return height_; }
  int32_t         size() const { return width_ * height_; }

  glm::dvec3 get( int32_t index ) const {
    return glm::dvec3( planes_[0].get( index ), planes_[1].get( index ), planes_[2].get( index ) );
  }
  T get( int32_t c, int32_t u, int32_t v ) const { return planes_[c].get( u, v ); }

  Image& operator*=( T scalar ) {
    for ( auto& plane : planes_ ) plane *= scalar;
    return *this;
  }
  Image operator*( T scalar ) const {
    Image result;
    result.resize( width_, height_ );
    for ( int32_t i = 0; i < N; i++ ) result[i] = this->planes_[i] * scalar;
    return result;
  }

  bool save( const std::string& prefix ) const {
    auto filename = prefix + "_" + std::to_string( width_ ) + "x" + std::to_string( height_ ) + "_" +
                    std::to_string( sizeof( T ) * 8 ) + "b_p444.rgb";
    std::ofstream file( filename, std::ios::binary );
    if ( !file.is_open() ) {
      printf( "Error: Unable to open file %s for writing.\n", filename.c_str() );
      return false;
    }
#if 0
    for ( const auto& plane : planes_ )
      file.write( reinterpret_cast<const char*>( plane.data() ), plane.size() * sizeof( T ) );
#else
    for ( const auto& plane : planes_ )
      for ( int32_t v = height_ - 1; v >= 0; v-- )
        file.write( reinterpret_cast<const char*>( plane.data( 0, v ) ), width_ * sizeof( T ) );
#endif
    if ( !file ) {
      printf( "Error: Failed to write image data to file: %s.\n", filename.c_str() );
      return false;
    }
    file.close();
    printf( "Save %s \n", filename.c_str() );
    return true;
  }

  void log( const std::string& str, int32_t u0 = 0, int32_t v0 = 0, int32_t number = 6 ) const {
    printf( "%s: %dx%d \n", str.c_str(), width_, height_ );
    for ( int32_t c = 0; c < N; c++ ) {
      printf( "%4s: ", c == 0 ? "R" : c == 1 ? "G" : c == 2 ? "B" : "A" );
      for ( int32_t u = u0; u < std::min( width_, u0 + number ); u++ )
        if constexpr ( std::is_floating_point<T>::value )
          printf( "%6d ", u );
        else
          printf( "%4d ", u );
    }
    printf( "\n" );
    for ( int32_t v = v0; v < std::min( height_, v0 + number ); v++ ) {
      for ( int32_t c = 0; c < N; c++ ) {
        printf( "%4d: ", v );
        for ( int32_t u = u0; u < std::min( width_, u0 + number ); u++ )

          if constexpr ( std::is_same_v<T, glm::vec4> )
            printf( "(%6.2f,%6.2f,%6.2f,%6.2f) ", static_cast<float>( planes_[c].get( u, v )[0] ),
                    static_cast<float>( planes_[c].get( u, v )[1] ), static_cast<float>( planes_[c].get( u, v )[2] ),
                    static_cast<float>( planes_[c].get( u, v )[3] ) );
          else if constexpr ( std::is_floating_point<T>::value )
            printf( "%6.2f ", static_cast<float>( planes_[c].get( u, v ) ) );
          else
            printf( "%4d ", static_cast<int32_t>( planes_[c].get( u, v ) ) );
      }
      printf( "\n" );
    }
    fflush( stdout );
  }

  T clamp( T v, T a, T b ) const { return ( ( v < a ) ? a : ( ( v > b ) ? b : v ) ); }

 private:
  int32_t               width_;
  int32_t               height_;
  std::vector<Plane<T>> planes_;
};

template <typename T, int N = 3>
class Video {
 public:
  Video( const Video& ) = default;
  Video( int32_t numFrames = 0, int32_t width = 0, int32_t height = 0, T value = 0 ) {
    resize( numFrames, width, height, value );
  }
  ~Video() = default;

  Video( const Video& videoA, const Video& videoB, bool butterfly = false ) {
    if ( videoA.height_ != videoB.height_ ) {
      printf( "Heigth are not the same \n" );
      fflush( stdout );
      exit( -1 );
    }
    if ( videoA.size() != videoB.size() ) {
      printf( "Size are not the same \n" );
      fflush( stdout );
      exit( -1 );
    }
    images_.clear();
    for ( size_t i = 0; i < videoA.size(); i++ ) images_.push_back( Image( videoA[i], videoB[i], butterfly ) );
    width_  = images_.back().width();
    height_ = images_.back().height();
  }

  void resize( int32_t numFrames, int32_t width, int32_t height, T value = 0 ) {
    width_  = width;
    height_ = height;
    images_.resize( numFrames );
    for ( size_t i = 0; i < images_.size(); i++ ) images_[i].resize( width, height, value );
  };

  void add( const Image<T, N>& image ) {
    if ( size() > 0 && ( width_ != image.width() || height_ != image.height() ) ) {
      printf( "Error: image size are not the same \n" );
      fflush( stdout );
      exit( 0 );
    }
    width_  = image.width();
    height_ = image.height();
    images_.push_back( image );
  }

  inline void zero() {
    for ( auto& el : images_ ) { el.zero(); }
  }
  inline void fill( T v ) {
    for ( auto& el : images_ ) { el.fill( v ); }
  }
  inline void clear() {
    for ( auto& el : images_ ) { el.clear(); }
  }

  Video& operator=( const Video& ) = default;
  template <typename D, int M>
  Video& operator=( const Video<D, M>& src ) {
    resize( src.size(), src.width(), src.height() );
    for ( int32_t i = 0; i < src.size(); i++ ) { images_[i] = src[i]; }
    return *this;
  }

  void swap( Video<T, N>& src ) {
    std::swap( width_, src.width_ );
    std::swap( height_, src.height_ );
    std::swap( images_, src.images_ );
  }

  Image<T>&       plane( int32_t index ) { return images_[index]; }
  const Image<T>& plane( int32_t index ) const { return images_[index]; }
  Image<T>&       back() { return images_.back(); }
  auto            begin() { return images_.begin(); }
  auto            end() { return images_.end(); }
  auto            begin() const { return images_.cbegin(); }
  auto            end() const { return images_.cend(); }
  Image<T>&       operator[]( int32_t index ) { return images_[index]; }
  const Image<T>& operator[]( int32_t index ) const { return images_[index]; }
  int32_t         size() const { return int32_t( images_.size() ); }
  int32_t         width() const { return width_; }
  int32_t         height() const { return height_; }

  Video& operator+=( Image<T, N>& image ) {
    this->add( image );
    return *this;
  }
  Video& operator*=( T scalar ) {
    for ( auto& image : images_ ) image *= scalar;
    return *this;
  }
  Video operator*( T scalar ) const {
    Video result;
    result.resize( size(), width_, height_ );
    for ( int32_t i = 0; i < N; i++ ) result[i] = this->images_[i] * scalar;
    return result;
  }

  bool save( const std::string& prefix, const bool planar = false ) const {
    auto filename = prefix + "_" + std::to_string( width_ ) + "x" + std::to_string( height_ ) + "_" +
                    std::to_string( sizeof( T ) * 8 ) + "b_" + ( planar ? "p" : "i" ) + "444.rgb";
    std::ofstream file( filename, std::ios::binary );
    if ( !file.is_open() ) {
      printf( "Error: Unable to open file %s for writing.\n", filename.c_str() );
      return false;
    }
    if ( planar ) {
      for ( const auto& image : images_ )
        for ( const auto& plane : image )
          for ( int32_t v = height_ - 1; v >= 0; v-- )
            file.write( reinterpret_cast<const char*>( plane.data( 0, v ) ), width_ * sizeof( T ) );
    } else {
      for ( const auto& image : images_ )
        for ( int32_t v = height_ - 1; v >= 0; v-- )
          for ( int32_t u = 0; u < width_; u++ )
            for ( int32_t c = 0; c < 3; c++ ) {
              T value = image.get( c, u, v );
              file.write( reinterpret_cast<const char*>( &value ), sizeof( T ) );
            }
    }
    if ( !file ) {
      printf( "Error: Failed to write image data to file : %s.\n", filename.c_str() );
      return false;
    }
    file.close();
    printf( "Save %s \n", filename.c_str() );
    return true;
  }

  void log( const std::string& str, int32_t u0 = 0, int32_t v0 = 0, int32_t number = 6 ) const {
    printf( "%s: %d %dx%d \n", str.c_str(), size(), width_, height_ );
    for ( size_t i = 0; i < size(); i++ ) {
      printf( "Frame %4zu \n", i );
      for ( int32_t c = 0; c < N; c++ ) {
        printf( "%4s: ", c == 0 ? "R" : c == 1 ? "G" : c == 2 ? "B" : "A" );
        for ( int32_t u = u0; u < std::min( width_, u0 + number ); u++ )
          if constexpr ( std::is_floating_point<T>::value )
            printf( "%6d ", u );
          else
            printf( "%4d ", u );
      }
      printf( "\n" );
      for ( int32_t v = v0; v < std::min( height_, v0 + number ); v++ ) {
        for ( int32_t c = 0; c < N; c++ ) {
          printf( "%4d: ", v );
          for ( int32_t u = u0; u < std::min( width_, u0 + number ); u++ )
            if constexpr ( std::is_same_v<T, glm::vec4> )
              printf( "(%6.2f,%6.2f,%6.2f,%6.2f) ", static_cast<float>( images_[i][c].get( u, v )[0] ),
                      static_cast<float>( images_[i][c].get( u, v )[1] ),
                      static_cast<float>( images_[i][c].get( u, v )[2] ),
                      static_cast<float>( images_[i][c].get( u, v )[3] ) );
            else if constexpr ( std::is_floating_point<T>::value )
              printf( "%6.2f ", static_cast<float>( images_[i][c].get( u, v ) ) );
            else
              printf( "%3x ", static_cast<int32_t>( images_[i][c].get( u, v ) ) );
        }
        printf( "\n" );
      }
    }
    fflush( stdout );
  }

  T clamp( T v, T a, T b ) const { return ( ( v < a ) ? a : ( ( v > b ) ? b : v ) ); }

 private:
  int32_t                  width_;
  int32_t                  height_;
  std::vector<Image<T, N>> images_;
};