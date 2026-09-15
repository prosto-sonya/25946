#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>

extern char **environ;

typedef struct {
    int option;
    char *argument;
} option_event;

static void print_usage(FILE *stream, const char *program)
{
    fprintf(stream,
            "Usage: %s [-i] [-s] [-p] [-u] [-Ulimit] [-c] [-Csize] "
            "[-d] [-v] [-Vname=value]\n"
            "Options are executed from right to left and may be repeated.\n",
            program);
}

static char *copy_string(const char *source)
{
    size_t length;
    char *copy;

    if (source == NULL) {
        return NULL;
    }

    length = strlen(source) + 1;
    copy = malloc(length);
    if (copy != NULL) {
        memcpy(copy, source, length);
    }
    return copy;
}

static int add_event(option_event **events, size_t *count, size_t *capacity,
                     int option, const char *argument)
{
    option_event *resized;
    char *argument_copy = copy_string(argument);

    if (argument != NULL && argument_copy == NULL) {
        perror("malloc");
        return -1;
    }

    if (*count == *capacity) {
        size_t new_capacity = *capacity == 0 ? 8 : *capacity * 2;
        resized = realloc(*events, new_capacity * sizeof(**events));
        if (resized == NULL) {
            perror("realloc");
            free(argument_copy);
            return -1;
        }
        *events = resized;
        *capacity = new_capacity;
    }

    (*events)[*count].option = option;
    (*events)[*count].argument = argument_copy;
    ++*count;
    return 0;
}

static int parse_limit(const char *text, const char *option_name, rlim_t *value)
{
    char *end = NULL;
    unsigned long long parsed;

    if (text == NULL || *text == '\0' || *text == '-') {
        fprintf(stderr, "%s requires a non-negative integer\n", option_name);
        return -1;
    }

    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' ||
        (unsigned long long)(rlim_t)parsed != parsed) {
        fprintf(stderr, "invalid value for %s: %s\n", option_name, text);
        return -1;
    }

    *value = (rlim_t)parsed;
    return 0;
}

static void print_limit_value(const char *name, rlim_t value)
{
    if (value == RLIM_INFINITY) {
        printf("%s=unlimited\n", name);
    } else {
        printf("%s=%llu\n", name, (unsigned long long)value);
    }
}

static int print_ids(void)
{
    printf("real_uid=%lu\n", (unsigned long)getuid());
    printf("effective_uid=%lu\n", (unsigned long)geteuid());
    printf("real_gid=%lu\n", (unsigned long)getgid());
    printf("effective_gid=%lu\n", (unsigned long)getegid());
    return 0;
}

static int become_group_leader(void)
{
    if (setpgid(0, 0) == -1) {
        perror("setpgid");
        return -1;
    }
    printf("process_group_leader=1\n");
    return 0;
}

static int print_process_ids(void)
{
    printf("pid=%lu\n", (unsigned long)getpid());
    printf("ppid=%lu\n", (unsigned long)getppid());
    printf("pgid=%lu\n", (unsigned long)getpgrp());
    return 0;
}

static int print_resource_limit(int resource, const char *name)
{
    struct rlimit limit;

    if (getrlimit(resource, &limit) == -1) {
        perror("getrlimit");
        return -1;
    }
    print_limit_value(name, limit.rlim_cur);
    return 0;
}

static int set_resource_limit(int resource, const char *name,
                              const char *option_name, const char *argument)
{
    struct rlimit limit;
    rlim_t value;

    if (parse_limit(argument, option_name, &value) == -1) {
        return -1;
    }
    if (getrlimit(resource, &limit) == -1) {
        perror("getrlimit");
        return -1;
    }
    if (limit.rlim_max != RLIM_INFINITY && value > limit.rlim_max) {
        fprintf(stderr, "%s exceeds the hard limit\n", option_name);
        return -1;
    }

    limit.rlim_cur = value;
    if (setrlimit(resource, &limit) == -1) {
        perror("setrlimit");
        return -1;
    }
    printf("%s_set=%llu\n", name, (unsigned long long)value);
    return 0;
}

static int print_directory(void)
{
    char *directory = getcwd(NULL, 0);

    if (directory == NULL) {
        perror("getcwd");
        return -1;
    }
    printf("current_working_directory=%s\n", directory);
    free(directory);
    return 0;
}

static int print_environment(void)
{
    char **entry;

    for (entry = environ; entry != NULL && *entry != NULL; ++entry) {
        puts(*entry);
    }
    return 0;
}

static int set_environment_variable(const char *argument)
{
    const char *separator;
    size_t name_length;
    char *name;

    if (argument == NULL || (separator = strchr(argument, '=')) == NULL ||
        separator == argument) {
        fprintf(stderr, "-V requires NAME=VALUE\n");
        return -1;
    }

    name_length = (size_t)(separator - argument);
    name = malloc(name_length + 1);
    if (name == NULL) {
        perror("malloc");
        return -1;
    }
    memcpy(name, argument, name_length);
    name[name_length] = '\0';

    if (setenv(name, separator + 1, 1) == -1) {
        perror("setenv");
        free(name);
        return -1;
    }
    printf("environment_set=%s=%s\n", name, separator + 1);
    free(name);
    return 0;
}

static int execute_event(const option_event *event)
{
    switch (event->option) {
    case 'i':
        return print_ids();
    case 's':
        return become_group_leader();
    case 'p':
        return print_process_ids();
    case 'u':
        return print_resource_limit(RLIMIT_NOFILE, "open_file_limit");
    case 'U':
        return set_resource_limit(RLIMIT_NOFILE, "open_file_limit", "-U",
                                  event->argument);
    case 'c':
        return print_resource_limit(RLIMIT_CORE, "core_size_limit_bytes");
    case 'C':
        return set_resource_limit(RLIMIT_CORE, "core_size_limit_bytes", "-C",
                                  event->argument);
    case 'd':
        return print_directory();
    case 'v':
        return print_environment();
    case 'V':
        return set_environment_variable(event->argument);
    default:
        fprintf(stderr, "internal error: unsupported option -%c\n",
                event->option);
        return -1;
    }
}

int main(int argc, char **argv)
{
    option_event *events = NULL;
    size_t count = 0;
    size_t capacity = 0;
    int option;
    int status = 0;
    size_t index;

    opterr = 0;
    while ((option = getopt(argc, argv, ":ispuU:cC:dvV:")) != -1) {
        if (option == ':') {
            fprintf(stderr, "option -%c requires an argument\n", optopt);
            print_usage(stderr, argv[0]);
            status = 2;
            goto cleanup;
        }
        if (option == '?') {
            if (optopt == '-') {
                continue;
            }
            fprintf(stderr, "invalid option: -%c\n", optopt);
            print_usage(stderr, argv[0]);
            status = 2;
            goto cleanup;
        }
        if (add_event(&events, &count, &capacity, option, optarg) == -1) {
            status = 1;
            goto cleanup;
        }
    }

    if (optind < argc) {
        fprintf(stderr, "unexpected operand: %s\n", argv[optind]);
        print_usage(stderr, argv[0]);
        status = 2;
        goto cleanup;
    }
    if (count == 0) {
        print_usage(stdout, argv[0]);
        goto cleanup;
    }

    for (index = count; index > 0; --index) {
        if (execute_event(&events[index - 1]) == -1) {
            status = 1;
            break;
        }
    }

cleanup:
    for (index = 0; index < count; ++index) {
        free(events[index].argument);
    }
    free(events);
    return status;
}
