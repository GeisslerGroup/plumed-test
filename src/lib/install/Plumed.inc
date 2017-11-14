PLUMED_RUNTIME_LOAD= "/usr/local/lib/plumed/obj/Plumed.o" -ldl  
PLUMED_STATIC_LOAD= "/usr/local/lib/plumed/obj/kernel.o" "/usr/local/lib/plumed/obj/PlumedStatic.o" -ldl  -lz -lmatheval -ldl   -rdynamic -fopenmp 
PLUMED_SHARED_LOAD= "/usr/local/lib/libplumed.so" -ldl  
PLUMED_RUNTIME_DEPENDENCIES=
PLUMED_STATIC_DEPENDENCIES= "/usr/local/lib/plumed/obj/kernel.o" "/usr/local/lib/plumed/obj/PlumedStatic.o"
PLUMED_SHARED_DEPENDENCIES= "/usr/local/lib/libplumed.so"
