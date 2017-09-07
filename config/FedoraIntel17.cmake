# module load mpi
set(CMAKE_C_COMPILER  mpiicc)
set(CMAKE_CXX_COMPILER  mpiicpc)

set(GNU_OPTS "-DADD_ -DINLINE_ALL=inline -DDISABLE_TIMER") 
set(INTEL_OPTS "-restrict -unroll -O3 -ip -xAVX -qopenmp -Wno-deprecated")
set(CMAKE_CXX_FLAGS "${INTEL_OPTS} ${GNU_OPTS} -std=c++98")
set(CMAKE_C_FLAGS "${INTEL_OPTS} -std=c99")

# copied from LinuxIntel.cmake
set(ENABLE_OPENMP 1)
set(HAVE_MPI 1)
set(HAVE_SSE 1)
set(HAVE_SSE2 1)
set(HAVE_SSE3 1)
set(HAVE_SSSE3 1)
set(HAVE_SSE41 0)
set(USE_PREFETCH 1)
set(PREFETCH_AHEAD 10)
set(HAVE_MKL 1)
set(HAVE_MKL_VML 1)

include_directories($ENV{MKLROOT}/include)
link_libraries(-L$ENV{MKLROOT}/lib/intel64 -mkl=sequential)

INCLUDE(Platform/UnixPaths)

SET(CMAKE_CXX_LINK_SHARED_LIBRARY)
SET(CMAKE_CXX_LINK_MODULE_LIBRARY)
SET(CMAKE_C_LINK_SHARED_LIBRARY)
SET(CMAKE_C_LINK_MODULE_LIBRARY)

# show compile commands
SET(CMAKE_EXPORT_COMPILE_COMMANDS)

# remove svn and fftw locations in CMakeCache
