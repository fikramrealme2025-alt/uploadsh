#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <iostream>

// Embedded binaries (generated with: xxd -i <file>.so > <file>.h)
#include "inject.h"      // unsigned char inject[]      / inject_len       (aarch64 injector)
#include "libGONG64.h"   // unsigned char libGONG64_so[] / libGONG64_so_len
#include "libGONG32.h"   // unsigned char libGONG32_so[] / libGONG32_so_len
#include "libMEOW64.h"   // unsigned char libMEOW64_so[] / libMEOW64_so_len
#include "libMEOW32.h"   // unsigned char libMEOW32_so[] / libMEOW32_so_len

// Optional: a 32-bit (armeabi-v7a) injector. If a file "inject32.h" exists it
// must define `unsigned char inject32[]` and `unsigned int inject32_len`.
#if defined(__has_include)
#  if __has_include("inject32.h")
#    include "inject32.h"
#    define HAVE_INJECT32 1
#  endif
#endif

struct Payload {
    const unsigned char* data;
    size_t len;
};

struct Target {
    const char* name;      // label shown in the menu
    const char* pkg;       // package name of the game
    const char* so_name;   // file name used in /data/local/tmp
    Payload lib64;
    Payload lib32;
};

static const Target kTargets[] = {
    { "GONG", "com.mobilechess.gp", "GONG.so",
      { libGONG64_so, libGONG64_so_len }, { libGONG32_so, libGONG32_so_len } },
    { "MEOW", "com.mobile.legends", "MEOW.so",
      { libMEOW64_so, libMEOW64_so_len }, { libMEOW32_so, libMEOW32_so_len } },
};
static const int kTargetCount = sizeof(kTargets) / sizeof(kTargets[0]);

static bool dump_file(const std::string& path, const unsigned char* data, size_t len) {
    FILE* fp = fopen(path.c_str(), "wb");
    if (!fp) {
        perror("fopen");
        return false;
    }
    size_t w = fwrite(data, 1, len, fp);
    fclose(fp);
    if (w != len) return false;
    chmod(path.c_str(), 0755);
    return true;
}

static void type_effect(const std::string& text, int delay_us = 40000) {
    for (char c : text) {
        std::cout << c << std::flush;
        usleep(delay_us);
    }
}

static void loading_progress() {
    std::string text = "[*] Loading components: ";
    std::cout << text << std::flush;

    const int total = 20;
    for (int i = 0; i <= total; i++) {
        std::cout << "\r" << text << "[";
        for (int j = 0; j < i; j++) std::cout << "#";
        for (int j = i; j < total; j++) std::cout << " ";
        std::cout << "] " << (i * 5) << "%" << std::flush;
        usleep(150000);
    }
    std::cout << "\n[+] Done!\n";
}

// Find the pid of a process whose cmdline is exactly `pkg` (main process).
static int find_pid(const char* pkg) {
    DIR* d = opendir("/proc");
    if (!d) return -1;
    int found = -1;
    while (dirent* e = readdir(d)) {
        int pid = atoi(e->d_name);
        if (pid <= 0) continue;
        char path[64];
        snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
        FILE* fp = fopen(path, "r");
        if (!fp) continue;
        char buf[256] = {0};
        size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
        fclose(fp);
        if (n > 0 && strcmp(buf, pkg) == 0) {
            found = pid;
            break;
        }
    }
    closedir(d);
    return found;
}

// Returns 64, 32, or 0 if it cannot be determined (reads the ELF class of /proc/<pid>/exe).
static int process_bits(int pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/exe", pid);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    unsigned char hdr[5] = {0};
    ssize_t n = read(fd, hdr, sizeof(hdr));
    close(fd);
    if (n < 5 || hdr[0] != 0x7f || hdr[1] != 'E' || hdr[2] != 'L' || hdr[3] != 'F') return 0;
    return hdr[4] == 2 ? 64 : (hdr[4] == 1 ? 32 : 0);
}

static int choose_target(int argc, char** argv) {
    if (argc > 1) {
        for (int i = 0; i < kTargetCount; i++) {
            if (strcasecmp(argv[1], kTargets[i].name) == 0 ||
                strcmp(argv[1], kTargets[i].pkg) == 0)
                return i;
        }
        std::cout << "[-] Unknown target: " << argv[1] << "\n";
    }
    std::cout << "\nSelect target:\n";
    for (int i = 0; i < kTargetCount; i++)
        std::cout << "  " << (i + 1) << ". " << kTargets[i].name << "  (" << kTargets[i].pkg << ")\n";
    std::cout << "> " << std::flush;
    int sel = 0;
    std::cin >> sel;
    return (sel >= 1 && sel <= kTargetCount) ? sel - 1 : -1;
}

int main(int argc, char** argv) {
    system("clear");

    type_effect("==============================\n");
    type_effect("https://t.me/@SiwaMods\n");
    type_effect("==============================\n");

    int idx = choose_target(argc, argv);
    if (idx < 0) {
        std::cout << "[-] Invalid selection.\n";
        return 1;
    }
    const Target& t = kTargets[idx];

    std::cout << "[+] Target : " << t.name << " (" << t.pkg << ")\n";
    std::cout << "[+] Starting Injection....\n";
    loading_progress();

    // Wait (up to ~60 s) for the game process to be running, then detect 32/64-bit.
    int pid = find_pid(t.pkg);
    if (pid < 0) {
        std::cout << "[*] Waiting for " << t.pkg << " to start (open the game now)...\n";
        for (int i = 0; i < 120 && pid < 0; i++) {
            usleep(500000);
            pid = find_pid(t.pkg);
        }
    }
    if (pid < 0) {
        std::cout << "[-] FAILED. " << t.pkg << " is not running.\n";
        return 1;
    }
    sleep(2);  // let the process finish loading before attaching

    int bits = process_bits(pid);
    if (bits == 0) {
        std::cout << "[-] FAILED. Cannot read /proc/" << pid << "/exe (run as root).\n";
        return 1;
    }
    std::cout << "[+] Game pid " << pid << ", " << bits << "-bit\n";

    const unsigned char* inj_data = inject;
    size_t inj_len = inject_len;
    const Payload& lib = (bits == 64) ? t.lib64 : t.lib32;

    if (bits == 32) {
#ifdef HAVE_INJECT32
        inj_data = inject32;
        inj_len = inject32_len;
#else
        std::cout << "[-] FAILED. Game is 32-bit but no 32-bit injector is built in.\n"
                  << "    Add jni/inject32.h (xxd -i of an armeabi-v7a injector) and rebuild.\n";
        return 1;
#endif
    }

    const std::string inject_path = "/data/local/tmp/inject";
    const std::string lib_path = std::string("/data/local/tmp/") + t.so_name;

    if (!dump_file(inject_path, inj_data, inj_len) || !dump_file(lib_path, lib.data, lib.len)) {
        std::cout << "[-] FAILED. Cannot write to /data/local/tmp.\n";
        unlink(inject_path.c_str());
        unlink(lib_path.c_str());
        return 1;
    }

    std::string cmd = inject_path + " -f -n " + t.pkg + " -so " + lib_path + " > /dev/null 2>&1";
    int status = system(cmd.c_str());
    if (status == 0) {
        std::cout << "[+] SUCCESS.\n";
    } else {
        std::cout << "[-] FAILED.\n";
    }
    unlink(inject_path.c_str());
    unlink(lib_path.c_str());
    return status == 0 ? 0 : 1;
}
