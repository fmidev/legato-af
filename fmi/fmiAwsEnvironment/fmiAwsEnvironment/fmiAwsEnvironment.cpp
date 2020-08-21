#include "legato.h"
#include "le_log.h"
#include <string>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>

void check(int err);
void do_mkdir(const char* fn);
void do_make_link(const char* dest, const char* link);
void make_overlay(const std::string& dest, const std::string& overlay_dir, const std::string& overlay_work_dir);

#define APP_RO_PREFIX "/legato/systems/current/apps/fmiAwsEnvironment/read-only"

COMPONENT_INIT
{
    struct stat buf;

    const char* sftp_server = APP_RO_PREFIX "/bin/sftp-server";
    //const char* socat = APP_RO_PREFIX "/bin/socat";
    //const char* minicom = APP_RO_PREFIX "/bin/minicom";

    if (stat("/usr/lib/openssh", &buf) != 0) {
        make_overlay("/usr/lib", "/tmp/fmi_ulib", "/tmp/fmi_ulib_mk");
        mkdir("/usr/lib/openssh", 0755);
        do_make_link(sftp_server, "/usr/lib/openssh/");
    }

    if (stat("/usr/libexec/sftp-server", &buf) != 0) {
        make_overlay("/usr/libexec", "/tmp/fmi-libexec", "/tmp/fmi-libexec_wk");
        do_make_link(sftp_server, "/usr/libexec/sftp-server");
    }

    //make_overlay("/usr/local", "/tmp/fmi-ulocal", "/tmp/fmi-ulocal_wk");
    //do_mkdir("/usr/local/bin");
    //do_make_link(socat, "/usr/local/bin/socat");
    //do_make_link(minicom, "/usr/local/bin/minicom");
}


void make_overlay(const std::string& dest, const std::string& overlay_dir, const std::string& overlay_work_dir)
{
    std::ostringstream cmd;

    cmd << "mkdir -p " << overlay_dir << ' ' << overlay_work_dir;
    if (0 != system((cmd.str().c_str()))) {
        LE_ERROR("Unable to create %s and/or %s", overlay_dir.c_str(), overlay_work_dir.c_str());
    }

    cmd.str("");
    cmd << "mount -t overlay -o upperdir=" << overlay_dir << ",lowerdir=" << dest
        << ",workdir=" << overlay_work_dir << " overlay " << dest;
    if (0 == system(cmd.str().c_str())) {
        // Do nothing
    } else {
        cmd.str("");
        cmd << "mount -t aufs -o dirs=" << overlay_dir << "=rw:" << dest << "=ro"
            << " aufs " << dest;
        if (0 == system(cmd.str().c_str())) {
            // Do nothing
        } else {
            LE_ERROR("Unable to mount overlay over %s", dest.c_str());
        }
    }
}

void do_mkdir(const char* fn)
{
    if (mkdir(fn, 0755) != 0) {
        LE_ERROR("Failed to create directory %s: %s", fn, strerror(errno));
    }
}

void do_make_link(const char* dest, const char* link)
{
    std::string tmp = link;
    LE_INFO("tmp=%s", tmp.c_str());
    if (*tmp.rend() == '/') {
        const char* s = strrchr(dest, '/');
        if (s == 0) s = dest;
        LE_INFO("basename=%s", s);
        tmp += s;
    }
    LE_INFO("tmp=%s", tmp.c_str());
    const char* link_name = tmp.c_str();
    if (symlink(dest, link_name) != 0) {
        LE_ERROR("symlink(%s, %s): %s", dest, link_name, strerror(errno));
    }
}
