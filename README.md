# Blang

Blang is a small, statically typed, garbage-collected, compiled imperative
language with lightweight syntax. It uses
[libgccjit](https://gcc.gnu.org/wiki/JIT) as a backend, which compiles to a
binary.

## Example

```
def sing_bottles_song(num:Int)
    for i in num..0 by -1
        if i == 0
            say "No more bottles of beer on the wall! :("
        else if i == 1
            say "One last bottle of beer on the wall"
        else
            say "$i bottles of beer on the wall"
    between
        say "Take one down, pass it around... "


sing_bottles_song 99
```

See [quickstart.bl](quickstart.bl) for a quick rundown of the language, or see
[the test folder](test/) for more comprehensive examples of each feature.

## Dependencies

The Blang compiler is written in C and uses
[libgccjit](https://gcc.gnu.org/onlinedocs/jit/) as the backend for
compilation. Blang also uses the [Boehm garbage
collector](https://www.hboehm.info/gc/) for runtime garbage collection. (both
are available from your package manager of choice, for example: `pacman -S
libgccjit gc`). The compiler uses a few GCC extensions, so other C compilers
are not supported.

## Usage

To build the compiler, simply run `make`. To install the compiler run `sudo
make install`.

The REPL can be run by running the command `blang` (or `./blang` in this
directory). Blang files can be run directly via `blang myprogram.bl`, compiled
to a static executable via `blangc myprogram.bl -o myprogram`, or compiled to a
library module via `blangc -c myprogram.bl -o libmyprogram.so`.

Additional command line arguments can be found in the manpages (`man
./blang.1`) or by running `blang --help` or `blangc --help`.

## Language Features

[See docs/features.md for writeups of some of the features in blang.](docs/features.md) These include:

- Simple value semantics and mutability rules
- Memory safety (GC, compiler-enforced null pointer checks, and automatic array bounds checking)
- Simple type system with type inference
- Type-safe DSL strings
- Units of measure
- Better loops
- Vectorized math operations
- Low-overhead datastructures
- Structs, not OOP
- Module system
- Semantic versioning
- Percentages

## Usage

Once the necessary dependencies are installed, you can run `./blang` to get a
REPL or use `./blang your-file.bl` to run a file directly or `./blang -c
your-file.bl` to compile it into a binary called `your-file`. See `blang
--help` for full usage info.

## License

Blang is released under the MIT license with the Commons Clause, see
[LICENSE](LICENSE) for full details.
