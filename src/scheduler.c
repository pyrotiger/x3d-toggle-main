#include "scheduler.h"
#include "error.h"
#include "libc.h"
#include "../build/xui.h"

#define PROC_KERNEL "/proc/sys/kernel/"

static int sysctl_write(const char *node, const char *value) {
    char path[128];
    size_t expected = strlen(PROC_KERNEL) + strlen(node);
    int n = printf_sn(path, sizeof(path), "%s%s", PROC_KERNEL, node);
    if (n < 0 || (size_t)n != expected) return -1;

    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;

    size_t len = strlen(value);
    size_t off = 0;
    while (off < len) {
        ssize_t bytes_written = write(fd, value + off, len - off);
        if (bytes_written < 0) {
            if (errno == EINTR) continue;
            close(fd);
            return -1;
        }
        off += (size_t)bytes_written;
    }

    if (close(fd) < 0) return -1;
    return 0;
}

bool scheduler_check(void) {
    return access(PROC_KERNEL "sched_bore", F_OK) == 0;
}

int scheduler_set(sched_t mode) {
    if (geteuid() != 0) return -1;

    if (mode == SCHED_GAMING) {
        if (sysctl_write("sched_cfs_bandwidth_slice_us", "3000") != 0) return -1;
        
        if (scheduler_check()) {
            if (sysctl_write("sched_bore", "1") != 0) return -1;
            if (sysctl_write("sched_bit_shift", "14") != 0) return -1; // Aggressive burst identification
        }
    } else {
        if (sysctl_write("sched_cfs_bandwidth_slice_us", "5000") != 0) return -1;
        
        if (scheduler_check()) {
            if (sysctl_write("sched_bore", "0") != 0) return -1;
            if (sysctl_write("sched_bit_shift", "12") != 0) return -1; // Default BORE shift
        }
    }
    
    return 0;
}

int cli_scheduler_gaming(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    int res = scheduler_set(SCHED_GAMING);
    if (res == 0) {
        printf_step("${ALRIGHT} Scheduler: ${COLOR_CYAN}GAMING${COLOR_RESET} profile applied.");
    } else {
        printf_step("${WARN} Scheduler: Failed to apply GAMING profile.");
    }
    return res;
}

int cli_scheduler_balanced(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    int res = scheduler_set(SCHED_BALANCED);
    if (res == 0) {
        printf_step("${ALRIGHT} Scheduler: ${COLOR_CYAN}BALANCED${COLOR_RESET} profile applied.");
    } else {
        printf_step("${WARN} Scheduler: Failed to apply BALANCED profile.");
    }
    return res;
}