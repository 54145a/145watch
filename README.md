## 145watch

> Cross-platform implementation of procps watch

```shell
winget install 54145a.145watch
```

[link](https://github.com/54145a/145watch/)

### Usage

```
Usage: watch [--help] [--version] [--beep] [--interval <seconds>] [--precise] [<command>]...

Positional arguments:
  <command>                 command to execute [nargs: 0 or more]

Optional arguments:
  -h, --help                shows help message and exits
  -v, --version             prints version information and exits
  -b, --beep                beep if command has a non-zero exit
  -n, --interval <seconds>  seconds to wait between updates, minimum is 0.1
  -p, --precise             run command in precise intervals
```

In the TUI, press `q` or `Esc` to quit.

### Example

```powershell
./145watch.exe -p -b -n 0.5 false
```

### Build

Requires [vcpkg](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started) and CMake 3.15+.

```shell
cmake -B build -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

Notes:

* `-DCMAKE_TOOLCHAIN_FILE` is only needed for the first configure; it is cached afterwards.
  If vcpkg moves, delete `build/` and configure again.
* Dependencies (`ftxui`, `argparse`) come from the `vcpkg.json` manifest and are pinned
  by `builtin-baseline`. The first configure compiles them (a few minutes); later ones are cached.
* If vcpkg reports `document schema version ... is not supported`, its binary is older than
  its checkout: run `./bootstrap-vcpkg.sh` (Linux/macOS) or `.\bootstrap-vcpkg.bat` (Windows).

Windows builds are produced by CI only; see [build.yml](.github/workflows/build.yml).

### Roadmap

* Basic functionality (80%)
* TUI (50%)
* Extended functionality (0%)

---

Contributions are welcomed.
