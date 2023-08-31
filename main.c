#include "getopt.h"

#include <stdint.h>
#include <stdio.h>

#define PROGNAME "msum"
#define VERSION "1.00"

struct pkg_header {
    uint32_t magic;
    uint32_t type;
    uint32_t unknown_data;
    uint32_t file_count;
    uint32_t entry_count;
    uint32_t garbage_data;
    uint32_t table_offset;
} __attribute__ ((packed, scalar_storage_order("big-endian")));

struct pkg_table_entry {
    uint32_t id;
    uint32_t filename_offset;
    uint32_t flags1;
    uint32_t flags2;
    uint32_t offset;
    uint32_t size;
    uint64_t padding;
} __attribute__ ((packed, scalar_storage_order("big-endian")));

int is_ps4_magic(const char *magic)
{
    static char pkg_magic[4] = { 0x7F, 0x43, 0x4E, 0x54 };
    for (int i = 0; i < 4; i++)
        if (magic[i] != pkg_magic[i])
            return 0;
    return 1;
}

int get_married(const char *filename, unsigned char *buf)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", filename);
        return -1;
    }

    char magic[4];
    if (fread(magic, 4, 1, file) != 1 || fseek(file, 0, SEEK_SET))
        goto read_error;
    if (!is_ps4_magic(magic)) {
        fprintf(stderr, "Not a PS4 PKG file: \"%s\".\n", filename);
        return -1;
    }

    struct pkg_header header;
    if (fread((void *) &header, sizeof(header), 1, file) != 1)
        goto read_error;
    if (fseek(file, header.table_offset, SEEK_SET))
        goto read_error;
    struct pkg_table_entry entry;
    if (fread((void *) &entry, sizeof(entry), 1, file) != 1)
        goto read_error;
    if (fseek(file, entry.offset + 32, SEEK_SET))
        goto read_error;
    if (fread(buf, 32, 1, file) != 1)
        goto read_error;

    fclose(file);
    return 0;

read_error:
    fprintf(stderr, "Could not read from file \"%s\".\n", filename);
    fclose(file);
    return -1;
}

void print_help(FILE *stream, const struct option *opts)
{
    fprintf(stream, "Usage: %s [OPTIONS] FILE [FILE ...]\n\n", PROGNAME);
    fprintf(stream,
        "This program takes PS4 PKG files and prints checksums.\n"
        "If checksums match, the PKG files are compatible with each other (\"married\").\n"
        "The program can also be run on partially downloaded files.\n"
    );
    fputs("\nOptions:\n", stream);
    print_options(stream, opts);
}

void print_version(void)
{
    puts("Version " VERSION "\n"
        "Get the latest version at \"https://github.com/hippie68/msum\".\n"
        "Report bugs and request features at \"https://github.com/hippie68/msum/issues\"."
    );
}

int main(int argc, char *argv[])
{
    static struct option opts[] = {
        { 'h', "help", NULL, "Print help information and quit." },
        { 'v', "version", NULL, "Print the current program version and quit." },
        { 0 }
    };

    if (argc < 2) {
        print_help(stderr, opts);
        return 1;
    }

    int opt;
    char *optarg;
    while ((opt = getopt(&argc, &argv, &optarg, opts)) != 0) {
        switch (opt) {
            case 'h':
                print_help(stdout, opts);
                return 0;
            case 'v':
                print_version();
                return 0;
            case '?':
                return 1;
        }
    }

    unsigned char buf[32];
    for (int i = 0; i < argc; i++) {
        if (get_married(argv[i], buf))
            continue;
        for (int c = 0; c < 32; c++)
            printf("%02X", buf[c]);
        printf(" %s\n", argv[i]);
    }
}
