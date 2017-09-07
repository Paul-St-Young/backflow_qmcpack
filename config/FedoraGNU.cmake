# module load mpi
set(CMAKE_C_COMPILER  mpicc)
set(CMAKE_CXX_COMPILER  mpicxx)

set(GNU_OPTS "-DADD_ -DINLINE_ALL=inline -DDISABLE_TIMER=1 -DUSE_REAL_STRUCT_FACTOR") 
set(GNU_FLAGS "-malign-double -fomit-frame-pointer -ffast-math -fopenmp -O3 -Drestrict=__restrict__ -finline-limit=1000 -fstrict-aliasing -funroll-all-loops -Wno-deprecated ")
set(CMAKE_CXX_FLAGS "${GNU_FLAGS} -std=c++98 -std=gnu++98 -ftemplate-depth-60 ${GNU_OPTS}")
set(CMAKE_C_FLAGS "${GNU_FLAGS} -std=c99 -std=gnu99")

set(ENABLE_OPENMP 1)

include_directories(/usr/include)
link_libraries(/usr/lib64/libblas.so)
link_libraries(/usr/lib64/liblapack.so)
