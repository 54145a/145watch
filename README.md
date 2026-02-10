## 145watch

Metronome for the Windows command line.

### Usage

```
Usage: 145watch [options] (<command> | false)
Options:
        -b, --beep      beep if command has a non-zero exit
        -n, --interval <seconds>        seconds to wait between updates, minimum is 0.1
        -p, --precise   run command in precise intervals

-h, --help      show help and exit
-v, --version   show version info and exit
```

### Build

This project uses CMake. See [https://cmake.org/cmake/help/latest/manual/cmake.1.html](https://cmake.org/cmake/help/latest/manual/cmake.1.html) for more instructions.

```shell
cmake -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE
cmake --build build
```

Contributions are welcomed.

![not by ai](https://i0.hdslb.com/bfs/new_dyn/10ba44a4a35b7a3d56461ea5358143323546740225476826.png)