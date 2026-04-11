## 145watch

Metronome for the Windows command line.

[link](https://github.com/54145a/145watch/)

### Usage

```
Usage: watch [options] (<command> | false)
Options:
        -b, --beep      beep if command has a non-zero exit
        -n, --interval <seconds>        seconds to wait between updates, minimum is 0.1
        -p, --precise   run command in precise intervals

-h, --help      show help and exit
-v, --version   show version info and exit
```

### Example

```powershell
./145watch.exe -p -b -n 0.5 false
```

### Build

#### Windows

Requires: vcpkg

```shell
cd build
cmake .. --preset=vcpkg
cmake --build .
```

#### POSIX

Requires: ncurses

```shell
cd build
cmake ..
cmake --build .
```

---

Contributions are welcomed.

![not by ai](https://i0.hdslb.com/bfs/new_dyn/9db9da973beb8dce14ecfe050e0a75713546740225476826.png)
