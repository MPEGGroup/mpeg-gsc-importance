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

class Timer {
 public:
  typedef std::tuple<std::string,
                     std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds>,
                     std::chrono::nanoseconds>
      TicToc;

  void tic( const std::string& name ) {
    auto it = std::find_if( m_ticTocList.begin(), m_ticTocList.end(),
                            [&name]( const TicToc& element ) { return std::get<0>( element ) == name; } );

    auto start = std::chrono::steady_clock::now();
    if ( it != m_ticTocList.end() ) {
      std::get<1>( *it ) = start;
    } else {
      m_ticTocList.push_back( std::make_tuple( name, start, std::chrono::nanoseconds( 0 ) ) );
    }
  }

  void toc( const std::string& name, const bool verbose = true ) {
    auto it = std::find_if( m_ticTocList.begin(), m_ticTocList.end(),
                            [&name]( const TicToc& element ) { return std::get<0>( element ) == name; } );

    if ( it != m_ticTocList.end() ) {
      auto end      = std::chrono::steady_clock::now();
      auto duration = end - std::get<1>( *it );
      std::get<2>( *it ) += duration;
      if ( verbose )
        std::cout << "Duration " << std::left << std::setw( 25 ) << name << ": time = " << std::right << std::setw( 15 )
                  << std::chrono::duration<double>( duration ).count() << " s Total = " << std::setw( 15 )
                  << std::chrono::duration<double>( std::get<2>( *it ) ).count() << " s\n";
    } else {
      std::cout << "Duration " << name << ": can't find clock\n";
    }
  }

  void reset( const std::string& name ) {
    auto it = std::find_if( m_ticTocList.begin(), m_ticTocList.end(),
                            [&name]( const TicToc& element ) { return std::get<0>( element ) == name; } );

    if ( it != m_ticTocList.end() ) { std::get<2>( *it ) = std::chrono::nanoseconds( 0 ); }
  }

  double getTime( const std::string& name ) const {
    auto it = std::find_if( m_ticTocList.begin(), m_ticTocList.end(),
                            [&name]( const TicToc& element ) { return std::get<0>( element ) == name; } );

    if ( it != m_ticTocList.end() ) {
      return ( std::get<2>( *it ) == std::chrono::nanoseconds::max() )
                 ? -1.0
                 : std::chrono::duration<double>( std::get<2>( *it ) ).count();
    }
    return -1.0;
  }

  void trace() const {
    std::cout << "Duration: \n";
    for ( const auto& el : m_ticTocList ) {
      std::cout << "  " << std::left << std::setw( 25 ) << std::get<0>( el ) << ": time = " << std::right
                << std::setw( 15 )
                << ( std::get<2>( el ) == std::chrono::nanoseconds::max()
                         ? -1.0
                         : std::chrono::duration<double>( std::get<2>( el ) ).count() )
                << " s\n";
    }
  }

 private:
  std::vector<TicToc> m_ticTocList;
};