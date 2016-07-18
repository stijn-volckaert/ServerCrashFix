rm ServerCrashFix_v11.o
rm ServerCrashFix_v11.so
rm ../System/ServerCrashFix_v11.so

# compile and output to this folder -> no linking yet!
gcc-2.95  -c -D__LINUX_X86__ -O2 -fomit-frame-pointer -march=pentium -D_REENTRANT -fPIC \
-DGPackage=ServerCrashFix_v11 -Werror -I../ServerCrashFix_v11/Inc -I../Core/Inc -I../Engine/Inc \
-o../ServerCrashFix_v11/ServerCrashFix_v11.o ../ServerCrashFix_v11/Src/ServerCrashFix_v11.cpp

# link with UT libs
# -Wl,-rpath,. -shared
# -lm -ldl -lnsl -lpthread
gcc-2.95  -shared -o ServerCrashFix_v11.so -Wl,-rpath,. \
-export-dynamic -Wl,-soname,ServerCrashFix_v11.so \
-lm -lc -ldl -lnsl -lpthread ../ServerCrashFix_v11/ServerCrashFix_v11.o  \
../System/Core.so ../System/Engine.so

cp ServerCrashFix_v11.so ../System/

