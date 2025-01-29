#ifndef ARGMIN_H
#define ARGMIN_H

#include <iostream>
#include <string>
#include <map>

// A small table describing each option
struct OptionSpec {
    const char* shortOpt;       // e.g. "-h"
    const char* longOpt;        // e.g. "--help"
    bool        requiresArgument;
    const char* description;
};

// OPTION SPECIFICATIONS:
//    Each entry has shortOpt, longOpt, requiresArgument, and a description
//
// EXAMPLE:
//
//    OptionSpec options[] = {
//        {"-h", "--help",   false, "Show this help message"},
//        {"-s", "--speed",  true,  "Set the speed (requires a value)"},
//        // Add more entries if needed...
//        {nullptr, nullptr, false, nullptr} // sentinel to mark end
//    };

int argmin(OptionSpec *options, int argc, char **argv, std::map<std::string, std::string> &parsedOptions);

#endif // ARGMIN_H
