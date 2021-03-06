OpenGeoSys5-EGS
============

[![Tag](https://img.shields.io/github/tag/norihiro-w/ogs5-egs.svg?style=flat-square)](https://github.com/norihiro-w/ogs5-egs/releases)
[![BSD License (modified)](http://img.shields.io/badge/license-BSD-blue.svg?style=flat-square)](https://github.com/norihiro-w/ogs5-egs/blob/master/LICENSE.txt)
[![Travis](https://img.shields.io/travis/norihiro-w/ogs5-egs.svg?style=flat-square)](https://travis-ci.org/norihiro-w/ogs5-egs)
[![Build status](https://ci.appveyor.com/api/projects/status/hiimukien0o5b856/branch/master?svg=true)](https://ci.appveyor.com/project/norihiro-w/ogs5-egs/branch/master)

This code is a branch of OpenGeoSys5 specially developed for modeling deep geothermal reservoirs. The code is based on the OGS official version 5.3.3 and will not support all the features in the official version (https://github.com/ufz/ogs5).

For more information, please visit http://norihiro-w.github.io/ogs5-egs

## OpenGeoSys project ##

[OpenGeoSys][ogs] (OGS) is a scientific open source project for the development of
numerical methods for the simulation of thermo-hydro-mechanical-chemical
(THMC) processes in porous and fractured media. OGS is implemented in C++, it
is object-oriented with an focus on the numerical solution of coupled multi-field
problems (multi-physics). Parallel versions of OGS are available relying on
both MPI and OpenMP concepts. Application areas of OGS are currently CO2
sequestration, geothermal energy, water resources management, hydrology and
waste deposition. OGS is developed by the
[OpenGeoSys Community][ogs].

- General homepage: http://www.opengeosys.org
- Wiki: https://svn.ufz.de/ogs
- Build instructions: https://docs.opengeosys.org/docs/devguide5/getting-started/introduction


## License ##

OpenGeoSys is distributed under a modified BSD License which encourages users to
attribute the work of the OpenGeoSys Community especially in scientific
publications. See the [LICENSE.txt][license-source] for the license text.


## External library ##

Current implementation uses the following external libraries:
- [Eigen](http://eigen.tuxfamily.org)
- [ExprTk](http://www.partow.net/programming/exprtk/index.html) 
- [Google Test](https://github.com/google/googletest)
- [Lis](http://www.ssisc.org/lis)
- [PARALUTION](http://www.paralution.com)
- [PETSc](https://www.mcs.anl.gov/petsc)


## Quickstart ##

For Windows user, the executable file might be available from [the GitHub release page](https://github.com/norihiro-w/ogs5-egs/releases/latest). For other environments, one needs to compile the code by themselves.

Prerequisite for the standard build
- [CMake](https://cmake.org/)
- C++ compilier supporting C++11 (e.g. GCC/Clang on *nix, Visual Studio on Windows)
- [Lis library](http://www.ssisc.org/lis): Please use [v1.2.70](http://www.ssisc.org/lis/dl/lis-1.2.70.tar.gz). The newer versions are not supported yet.

Compiling OGS

For *nix users,
``` bash
cd [source-directory]
mkdir build
cd build
cmake .. -DOGS_FEM_LIS=ON -DLIS_INCLUDE_DIR=[path to lis include dir] -DLIS_LIBRARIES=[path to lis library]
make
```

For Windows Visual Studio users,
``` bash
cd [source-directory]
mkdir build
cd build
cmake .. -DOGS_FEM_LIS=ON -DLIS_INCLUDE_DIR=[lis include dir] -DLIS_LIBRARIES=[lis library path]
cmake .. -G "Visual Studio 12 2013" -DOGS_FEM_LIS=ON -DLIS_INCLUDE_DIR=[path to lis include dir] -DLIS_LIBRARIES=[path to lis library]
cmake --build . --config Release
```

After the compilation finished, the OGS executable `ogs` will be generated under `build/bin` or `build/Release`. To start a simulation, command
``` bash
ogs (input file name without a file extension)
```

Examples of OGS input files can be found on https://github.com/norihiro-w/ogs5-egs-benchmarks.



[ogs]: http://www.opengeosys.org
[license-source]: https://github.com/norihiro-w/ogs5-egs/blob/master/LICENSE.txt
