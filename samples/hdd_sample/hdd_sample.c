#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ps2_hdd_driver.h>
#include <sifrpc.h>
#include <iopcontrol.h>
#include <sbv_patches.h>
#include <debug.h>
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <hdd-ioctl.h>
#include <io_common.h>

static void reset_IOP() {
    SifInitRpc(0);
    while (!SifIopReset(NULL, 0)) {};
    while (!SifIopSync()) {};
    SifInitRpc(0);
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
}

static void init_drivers() {
    init_hdd_driver(true, false);
}

static void deinit_drivers() {
    deinit_hdd_driver(true);
}

static void copy_directory(const char *src, const char *dst) {
    DIR *dp;
    struct dirent *ep;
    struct stat st;
    char src_path[1024], dst_path[1024];

    dp = opendir(src);
    if (dp != NULL) {
        mkdir(dst, 0777);
        while ((ep = readdir(dp)) != NULL) {
            if (strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0) {
                continue;
            }
            snprintf(src_path, sizeof(src_path), "%s/%s", src, ep->d_name);
            snprintf(dst_path, sizeof(dst_path), "%s/%s", dst, ep->d_name);
            stat(src_path, &st);
            if (S_ISDIR(st.st_mode)) {
                copy_directory(src_path, dst_path);
            } else {
                printf("Copying file: %s -> %s\n", src_path, dst_path);
                FILE *dst_file = fopen(dst_path, "wb");
                if (!dst_file) {
                    printf("Failed to open destination file: %s\n", dst_path);
                    continue;
                }
                FILE *src_file = fopen(src_path, "rb");
                if (!src_file) {
                    printf("Failed to open source file: %s\n", src_path);
                    fclose(dst_file);
                    continue;
                }
                char buffer[8192];
                size_t bytes;
                while ((bytes = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
                    printf("Copied %zu bytes\n", bytes);
                    fwrite(buffer, 1, bytes, dst_file);
                }
                fclose(src_file);
                fclose(dst_file);
            }
        }
        closedir(dp);
    } else {
        printf("Failed to open directory: %s\n", src);
    }
}

int main(int argc, char **argv) {
    if (argc < 1) {
        printf("Invalid arguments\n");
        return 1;
    }

    reset_IOP();
    init_scr();
    init_drivers();

    char *filename = strrchr(argv[0], '/');
    if (filename) {
        filename++;
    } else {
        filename = argv[0];
    }

    char *dot = strrchr(filename, '.');
    if (dot) {
        *dot = '\0';
    }


    scr_printf("\n\n\nHDD example!\n\n\n");
    scr_printf("Filename: %s\n", filename);
    int res = mount_hdd_partition("pfs0:", "hdd0:PP.KNAC_00001");
    printf("mount_hdd_partition pfs0: hdd0:KNAC_00001 res=%i\n", res);
    res = mount_hdd_partition("pfs1:", "hdd0:__common");
    printf("mount_hdd_partition pfs1: hdd0:__common res=%i\n", res);

    char src_path[1024], dst_path[1024];
    snprintf(src_path, sizeof(src_path), "pfs0:/PP.KNAC_00001/%s", filename);
    snprintf(dst_path, sizeof(dst_path), "pfs1:");

    copy_directory(src_path, dst_path);

    umount_hdd_partition("pfs0:");
    deinit_drivers();
    sleep(5);

    return 0;
}
