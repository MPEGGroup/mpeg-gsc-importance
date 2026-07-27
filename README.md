# Importance scores

This project computes per-Gaussian importance scores from one or more camera viewpoints. It reads 3D Gaussian splat PLY files, and writes one accumulated score per Gaussian into a .txt file.

## Dependencies

- A C++23 compiler
- CMake 3.17 or newer
- GLM 1.0.1
- program-options-lite
- OpenMP, unless configured with `USE_OPENMP=OFF`

## Build

```bash
./build.sh --compiler=gcc
```

Build without OpenMP:

```bash
./build.sh --compiler=gcc --noomp
```

Run developer checks:

```bash
./build.sh --format
./build.sh --cppcheck
```

The executable is written below the selected build directory, for example:

```text
build/Release/bin/importance-scores
```

## Usage

```bash
./build/Release/bin/importance-scores \
  --source=/path/to/frame_%04d.ply \
  --startFrame=0 \
  --frameCount=1 \
  --viewpoint=/path/to/viewpoints.txt \
  --impScore=/path/to/importance_scores.txt
```

When `--viewpoint` is omitted, camera positions embedded in the PLY header are used. Use the [gsTools software](https://git.mpeg.expert/MPEG/Explorations/GSC/gsc-software/mpeg-gsc-tools/-/tree/main/gsTools?ref_type=heads) to add camera position into ply files. 

`--downscaleFactor` and `--downscaleMinWidth` control the rasterization resolution.

Run `importance-scores --help` for the complete option list.

## Clean

```bash
./clear.sh
./clear.sh gcc
./clear.sh all
```

# Licence 

The copyright in this software is being made available under the BSD Licence, included below. This software may be subject to other third party and contributor rights, including patent rights, and no such rights are granted under this licence. 

Copyright (c) 2025, ISO/IEC All rights reserved. 

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met: 
* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer. 
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution. 
* Neither the name of the ISO/IEC nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 