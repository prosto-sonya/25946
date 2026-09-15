# Task 1

The program implements the process, resource-limit, working-directory and
environment options from practical task 1. Options are collected with
`getopt(3C)` and applied in reverse command-line order, so repeated options
are handled independently.

Build:

```sh
make
```

Run:

```sh
./options -i -p -d
```

The `-u`/`-U` options read and change the `RLIMIT_NOFILE` open-file limit,
as described in `Task_1/Task_1.txt`. The `-c`/`-C` core-file limit uses bytes
as required by the assignment. The setters can only reduce a limit or set it
up to the current hard limit; increasing a hard limit requires privilege.

Useful checks:

```sh
./options
./options -x
./options -Ubad
./options -VTASK1=right -v -VTASK1=left
./options -i-p
```
