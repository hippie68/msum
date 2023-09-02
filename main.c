// Copyright (c) 2023 hippie68 (https://github.com/hippie68/msum)
// Made with info from "https://www.psdevwiki.com/ps4/Package_Files".

// Prints checksums that indicate whether PS4 game and update PKG files are
// compatible with each other ("married").

#define _GNU_SOURCE

#include "getopt.h"
#include <dirent.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define PROGNAME "msum"
#define VERSION "1.01"
#define HOMEPAGE_LINK "https://github.com/hippie68/msum"
#define CONTACT_LINK "https://github.com/hippie68/msum/issues"

#define BUF_SIZE 32
#define LIST_MIN_SIZE 128
#ifdef _WIN32
#define DIR_SEPARATOR '\\'
#else
#define DIR_SEPARATOR '/'
#endif

struct pkg_header {
    uint32_t magic;
    uint32_t type;
    uint32_t unknown_data;
    uint32_t file_count;
    uint32_t entry_count;
    uint32_t garbage_data;
    uint32_t table_offset;
    uint32_t entry_data_size;
    uint64_t body_offset;
    uint64_t body_size;
    uint64_t content_offset;
    uint64_t content_size;
    unsigned char content_id[36];
    unsigned char padding[12];
    uint32_t drm_type;
    uint32_t content_type;
    uint32_t content_flags;
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

void print_checksum(const unsigned char *buf, const char *filename)
{
    for (int c = 0; c < BUF_SIZE; c++)
        printf("%02X", buf[c]);
    printf(" %s\n", filename);
}

inline static int is_ps4_magic(const char *magic)
{
    static char pkg_magic[4] = { 0x7F, 0x43, 0x4E, 0x54 };
    for (int i = 0; i < 4; i++)
        if (magic[i] != pkg_magic[i])
            return 0;
    return 1;
}

int get_checksum(const char *filename, unsigned char *buf)
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
        goto error;
    }

    struct pkg_header header;
    if (fread((void *) &header, sizeof(header), 1, file) != 1)
        goto read_error;

    if (header.content_type == 27) // DLC
        goto error;

    uint32_t target_id;
    switch (header.content_flags & 0x0F000000) {
        case 0x0A000000:
            target_id = 0x1001;
            break;
        case 0x02000000:
            target_id = 0x1008;
            break;
        default:
            goto error;
    }

    if (fseek(file, header.table_offset, SEEK_SET))
        goto read_error;
    struct pkg_table_entry entry;
    if (fread((void *) &entry, sizeof(entry), 1, file) != 1)
        goto read_error;
    uint32_t digests_offset = entry.offset;
    for (uint32_t i = 1; i < header.entry_count; i++) {
        if (fread((void *) &entry, sizeof(entry), 1, file) != 1)
            goto read_error;
        if (entry.id == target_id) {
            if (fseek(file, digests_offset + i * BUF_SIZE, SEEK_SET))
                goto read_error;
            if (fread(buf, BUF_SIZE, 1, file) != 1)
                goto read_error;
            fclose(file);
            return 0;
        }
    }
    goto error;

read_error:
    fprintf(stderr, "Could not read from file \"%s\".\n", filename);
error:
    fclose(file);
    return -1;
}

void print_help(FILE *stream, const struct option *opts)
{
    fprintf(stream,
        "Usage: %s [OPTIONS] FILE|DIRECTORY [FILE|DIRECTORY ...]\n\n"
        "This program takes PS4 game and update PKG files and prints checksums.\n"
        "If checksums match, the PKG files are compatible with each other (\"married\").\n"
        "The program can also be run on partially downloaded files.\n\n"
        "Options:\n", PROGNAME);
    print_options(stream, opts);
}

void print_version(void)
{
    printf("Version " VERSION ", build date: %s\n"
        "Get the latest version at \"" HOMEPAGE_LINK "\".\n"
        "Report bugs and request features at \"" CONTACT_LINK "\".\n",
        __DATE__);
}

void exit_oom(void)
{
    fprintf(stderr, "Out of memory.\n");
    exit(EXIT_FAILURE);
}

static int qsort_compare_strings(const void *p, const void *q)
{
    return strcmp(*(const char **)p, *(const char **)q);
}

inline static int is_root(const char *dir)
{
    return dir[0] == DIR_SEPARATOR && dir[1] == '\0';
}

void add_to_list(char ***list, size_t *list_count, size_t *list_count_max,
    const char *entry_name, const char *cur_dir)
{
    size_t size;
    if (is_root(cur_dir))
        size = 1 + strlen(entry_name) + 1;
    else
        size = strlen(cur_dir) + 1 + strlen(entry_name) + 1;
    if (((*list)[*list_count] = malloc(size)) == NULL)
        exit_oom();

    sprintf((*list)[*list_count], "%s%c%s", is_root(cur_dir) ? "" : cur_dir,
        DIR_SEPARATOR, entry_name);
    (*list_count)++;
    if (*list_count == *list_count_max) {
        *list_count_max *= 2;
        void *new = realloc(*list, sizeof(void *) * *list_count_max);
        if (new == NULL)
            exit_oom();
        *list = new;
    }
}

void parse_dir(char *cur_dir, unsigned char *buf, int recursion)
{
    DIR *dir;
    struct dirent *dir_entry;

    size_t dir_count_max = LIST_MIN_SIZE;
    size_t file_count_max = LIST_MIN_SIZE;
    char **dirs = NULL;
    char **files;
    size_t dir_count = 0, file_count = 0;

    // Remove trailing directory separators.
    int len = strlen(cur_dir);
    while (len > 1 && cur_dir[len - 1] == DIR_SEPARATOR) {
        cur_dir[len - 1] = '\0';
        len--;
    }

    dir = opendir(cur_dir);
    if (dir == NULL)
        return;

    if (recursion) {
        dirs = malloc(sizeof(void *) * dir_count_max);
        if (dirs == NULL)
            exit_oom();
    }
    files = malloc(sizeof(void *) * file_count_max);
    if (files == NULL)
        exit_oom();

    // Read all directory entries to put them in lists.
    while ((dir_entry = readdir(dir)) != NULL) {
#ifdef _WIN32 // MinGW does not know .d_type.
        struct stat statbuf;
        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s%c%s", is_root(cur_dir) ? "" : cur_dir,
            DIR_SEPARATOR, dir_entry->d_name);
        if (stat(path, &statbuf) == -1) {
            fprintf(stderr, "Could not read file system information: \"%s\".\n",
                path);
            continue;
        }
        if (S_ISDIR(statbuf.st_mode)) {
#else
        if (dir_entry->d_type == DT_DIR) { // Entry is a directory.
#endif
            if (recursion
                && dir_entry->d_name[0] != '.'
                && dir_entry->d_name[0] != '$') // Exclude system dirs.
            {
                add_to_list(&dirs, &dir_count, &dir_count_max,
                    dir_entry->d_name, cur_dir);
            }
        } else { // Entry is not a directory.
            char *file_extension = strrchr(dir_entry->d_name, '.');
            if (file_extension != NULL
                && strcasecmp(file_extension, ".pkg") == 0)
            {
                add_to_list(&files, &file_count, &file_count_max,
                    dir_entry->d_name, cur_dir);
            }
        }
    }

    qsort(files, file_count, sizeof(char *), qsort_compare_strings);

    for (size_t i = 0; i < file_count; i++) {
        if (!get_checksum(files[i], buf))
            print_checksum(buf, files[i]);
        free(files[i]);
    }

    if (recursion) {
        qsort(dirs, dir_count, sizeof(char *), qsort_compare_strings);
        for (size_t i = 0; i < dir_count; i++) {
            parse_dir(dirs[i], buf, recursion);
            free(dirs[i]);
        }
    }

    closedir(dir);
    free(dirs);
    free(files);
}

inline static int is_dir(const char *filename)
{
    struct stat sb;
    if (stat(filename, &sb))
        return 0;

    return S_ISDIR(sb.st_mode);
}

int main(int argc, char *argv[])
{
    static struct option opts[] = {
        { 'h', "help", NULL, "Print help information and quit." },
        { 'r', "recursive", NULL, "Traverse subdirectories recursively." },
        { 'v', "version", NULL, "Print the current program version and quit." },
        { 0 }
    };

    int option_recursive = 0;

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
            case 'r':
                option_recursive = 1;
                break;
            case 'v':
                print_version();
                return 0;
            case '?':
                return 1;
        }
    }

    unsigned char buf[BUF_SIZE];
    for (int i = 0; i < argc; i++) {
        if (is_dir(argv[i])) {
            parse_dir(argv[i], buf, option_recursive);
        } else {
            if (get_checksum(argv[i], buf))
                continue;
            print_checksum(buf, argv[i]);
        }
    }
}
