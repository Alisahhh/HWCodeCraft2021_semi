# 2021 华为软挑

## SDK 使用方法

### Linux 下

- 使用 CMake 或支持 CMake 的 IDE 运行；
- 使用 `sh build.sh` 编译，在 `bin` 目录下获取二进制文件；
- 使用 `sh build_and_run.sh` 编译并运行；
- 使用 `sh CodeCraft_zip.sh` 编译并打包为符合平台格式要求的压缩文件。

手动 CMake 的方法：

- 在 `src` 下新建文件夹 `build`：`mkdir build`；
- 切换到 `build` 中：`cd build`；
- 运行 CMake：`cmake ../CodeCraft-2021`；
- 编译：`make`；
- 在此目录下获得二进制文件 `CodeCraft-2021`。

### Windows 下

- 使用 CMake 或支持 CMake 的 IDE 运行。

手动 CMake 的方法：

- 在 `src` 下新建文件夹 `build`：`mkdir build`；
- 切换到 `build` 中：`cd build`；
- 运行 CMake：`cmake ../CodeCraft-2021 -G "MinGW Makefiles"`；
- 编译：`make`（部分 MinGW 中的 make 可能为其他文件名，如 `mingw32-make.exe`，请确认自己电脑中的文件名）；
- 在此目录下获得二进制文件 `CodeCraft-2021.exe`。