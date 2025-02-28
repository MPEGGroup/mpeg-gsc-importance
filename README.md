# MPEG Gaussian splat compression metrics 

Mpeg-gsc-metric computes image-based metrics between two sequences of 3D gaussian splat objects.

# Cloning and Building Instructions

The source code can be obtained by: 
```
git clone https://git.mpeg.expert/MPEG/3dgh/GSC/gsc-software/mpeg-gsc-metrics.git`
```

The software supports Windows, MacOs and Linux operating systems. Compilation can be done as follows:
* Linux:
  * to build use the command `build.sh`.
  * to clean all object use the command `clear.sh`.
* Windows:
  * to build use the CMAKE command or `build.sh` or open MSVC project ./build/Release/PccAppRenderer.sln
  * to clean all object use the command `clear.sh`.
* MacOs:
  * to build use the CMAKE command or `build.sh` or open Xcode.
  * to clean all object use the command `clear.sh`.

Cmake commands can be used to build software:

``` 
cd mpeg-gsc-metric
mkdir build 
cd ./build/
cmake .. 
cmake --build . --config Release --parallel 20 
```

The command `clear.sh all` can be used to remove all dependencies.


# Depensencies

Mpeg-gsc-metric project uses:
- OpenGL  
- [GLFW](https://github.com/glfw/glfw)
- [OpenGL Mathematics(GLM)](https://github.com/g-truc/glm.git)
- program-options-lite.

# Usage 

The mpeg-gsc-metric input parameters are listed below:

| --key=default value | Usage |
|-----|-----|
| **Global**            | |
| -h, --help=           | Print this help message <br> |
| -c, --config=         | Parse configuration file <br> |
| -v, --verbose=0       | Verbose <br> |
| **Input paths**       | |
| -a, --source=""       | Source 3DGS path <br> |
| -b, --decode=""       | Decode 3DGS path <br> |
| **Sequence information** | |
| -i, --startFrame=0    | First frame number <br> |
| -f, --frameCount=1    | Number of frames <br> |
| --width=1024          | Width of the generated images <br> |
| --height=768          | Height of the generated images <br> |
| **Viewpoints**        | |
| --viewpoint=""        | Viewpoint path <br> |
| -n, --numPoints=16    | Number of viewpoints <br> |
| --display=0           | Show 3D model and viewpoints <br> |
| --saveViewpoints=0    | Save the generated viewpoints <br> |
| **Rendering**         | |
| --cpu=0               | Use CPU rendering <br> |
| --float=0             | Use floatting point images <br> |
| **Options**           | |
| -s, --save=0          | Save generated videos <br> |


## Examples

The next section presents some example of command line used to compute metric. 

### Automatic metric between two 3DGS files

```
$ ./build/Release/bin/Release/mpeg-gsc-metrics.exe \
  -a src.ply \
  -b dec.ply
```

### Automatic viewpoints around the objects

```
$ ./build/Release/bin/Release/mpeg-gsc-metrics.exe \
     -a src_%04d.ply \
     -b dec_%04d.ply \
     -i 0 \
     -f 32 \
     -n 16 \
     --cpu 1 \
     -s \
     -v 
Metric : MSE RGB =     0.000000     0.000000     0.000000 PSNR RGB =   999.990000   999.990000   999.990000 MSE YUV =     0.000000     0.000000     0.000000 PSNR YUV =   999.990000   999.990000   999.990000
```

### Loaded viewpoints

```
$ ./build/Release/bin/Release/mpeg-gsc-metrics.exe \
     -a src.ply \
     -b dec.ply 
     --viewpoints ./config/viewpoints.txt
     --cpu 1 \
     -v  
Metric : MSE RGB =     0.000000     0.000000     0.000000 PSNR RGB =   999.990000   999.990000   999.990000 MSE YUV =     0.000000     0.000000     0.000000 PSNR YUV =   999.990000   999.990000   999.990000
```

# Notes
Please, don't hesitate to report me any issues with the renderer by the GitLab Issue Tracker or directly by [email](mailto:jricard@global.tencent.com).

# Licence 

The copyright in this software is being made available under the BSD Licence, included below. This software may be subject to other third party and contributor rights, including patent rights, and no such rights are granted under this licence. 

Copyright (c) 2025, ISO/IEC All rights reserved. 

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met: 
* Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer. 
* Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution. 
* Neither the name of the ISO/IEC nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 

# Contact 

* [Julien Ricard](mailto:jricard@global.tencent.com).