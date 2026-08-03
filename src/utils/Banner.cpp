#include "Banner.hpp"

// Normally supplied by the Makefile as -DVERSION. The fallback keeps a
// hand-rolled compile of a single file working.
#ifndef VERSION
# define VERSION "dev"
#endif

static const char *const LOGO =
    "\n"
    "███████╗███████╗██████╗ ██╗   ██╗ █████╗ ███╗   ██╗████████╗\n"
    "██╔════╝██╔════╝██╔══██╗██║   ██║██╔══██╗████╗  ██║╚══██╔══╝\n"
    "███████╗█████╗  ██████╔╝██║   ██║███████║██╔██╗ ██║   ██║\n"
    "╚════██║██╔══╝  ██╔══██╗╚██╗ ██╔╝██╔══██║██║╚██╗██║   ██║\n"
    "███████╗███████╗██║  ██║ ╚████╔╝ ██║  ██║██║ ╚████║   ██║\n"
    "╚══════╝╚══════╝╚═╝  ╚═╝  ╚═══╝  ╚═╝  ╚═╝╚═╝  ╚═══╝   ╚═╝\n"
    "\n";

// How many terminal columns the logo occupies. Hardcoded because the logo is
// fixed and its block characters are multi-byte in UTF-8: measuring the string
// would count bytes and come out at roughly three times the real width.
static const size_t LOGO_WIDTH = 60;

// Pad a line with the leading spaces that put it in the middle of the logo. Only
// ASCII gets centered here, so one character is one column.
static std::string centered(const std::string &line) {
    if (line.size() >= LOGO_WIDTH) return line;
    return std::string((LOGO_WIDTH - line.size()) / 2, ' ') + line;
}

std::string banner() {
    // The version sits on its own line so that the tagline above it stays put:
    // its length varies with how far the build is from the last release tag.
    // Centered at run time for the same reason — the padding is not known until
    // the version is.
    std::string version_line = std::string("v") + VERSION;

    return std::string(LOGO)
         + centered("Servant of the Web") + "\n"
         + centered(version_line) + "\n";
}
