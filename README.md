# msum

```
Usage: msum [OPTIONS] FILE|DIRECTORY [FILE|DIRECTORY ...]

This program takes PS4 game and update PKG files and prints checksums.
If checksums match, the PKG files are compatible with each other ("married").
The program can also be run on partially downloaded files.

Options:
  -h, --help       Print help information and quit.
  -r, --recursive  Traverse subdirectories recursively.
  -v, --version    Print the current program version and quit.
```

Sample output:
```
$ msum .
A755791CA4BD8D2AE7128B6FB704C50ADA93CA459EBEE8DCCFC07502822A9485 UP9000-CUSA28561_00-FORBIDDENWESTPS4-A0100-V0100.pkg
A755791CA4BD8D2AE7128B6FB704C50ADA93CA459EBEE8DCCFC07502822A9485 UP9000-CUSA28561_00-FORBIDDENWESTPS4-A0118-V0100.pkg
```
