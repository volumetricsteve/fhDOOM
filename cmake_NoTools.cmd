mkdir build
cd build
mkdir notools
cd notools
cmake -D ID_ALLOW_TOOLS=OFF -D ID_UNICODE=ON ../..