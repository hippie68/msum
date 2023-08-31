# msum

```
Usage: msum [OPTIONS] FILE [FILE ...]

This program takes PS4 PKG files and prints checksums.
If checksums match, the PKG files are compatible with each other ("married").
The program can also be run on partially downloaded files.

Options:
  -h, --help     Print help information and quit.
  -v, --version  Print the current program version and quit.
```

Sample output:
```
$ msum "Amazon Instant Video [v1.00].pkg"
B434DD9CB3C79196ECF7D1F68F8A2D188D078F2D33E9096B1D22B54E7AF26D6A Amazon Instant Video [v1.00].pkg
```
