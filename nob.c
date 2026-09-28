#define NOB_IMPLEMENTATION
#include "./nob.h"

int main(int argc, char* argv[]) {
    NOB_GO_REBUILD_URSELF(argc, argv);
    Nob_Cmd cmd = {0};

    nob_cmd_append(&cmd, "clang");
//    nob_cmd_append(&cmd, "-Wall", "-Wextra", "-ggdb");
    nob_cmd_append(&cmd, "-O3");
    nob_cmd_append(&cmd, "./main.c");
    nob_cmd_append(&cmd, "-lclang");
    nob_cmd_append(&cmd, "-o", "coogle2");

    if (!nob_cmd_run(&cmd)) return 1;
    return  0;
}
