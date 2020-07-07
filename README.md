MPTool
========================


### WSL with mingw

```shell
export PKG_CONFIG=i686-w64-mingw32-pkg-config
cmake -B build -GNinja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_TOOLCHAIN_FILE=/usr/share/mingw/toolchain-mingw32.cmake
```
