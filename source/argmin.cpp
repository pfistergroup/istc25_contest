#include <iostream>
#include <string>
#include <map>
#include "argmin.h"

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

int argmin(OptionSpec *options, int argc, char **argv, std::map<std::string, std::string> &parsedOptions)
{
    // Store parsed options in this map:
    //    Key = (longOpt name with the leading "--" removed), e.g. "help", "speed"
    //    Value = the argument if requiresArgument==true; otherwise ""
    //std::map<std::string, std::string> parsedOptions;

    // PARSE THE COMMAND LINE
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        bool matchedOption = false;

        // Try matching against each option in the table
        for (int j = 0; options[j].shortOpt != nullptr; ++j)
        {
            const std::string shortOpt = options[j].shortOpt;  // e.g. "-s"
            const std::string longOpt  = options[j].longOpt;   // e.g. "--speed"

            // We'll want to store in the map using something like "help" or "speed".
            // So let's extract the substring after the leading "--".
            std::string longOptKey = longOpt;
            if (longOptKey.rfind("--", 0) == 0) {
                longOptKey = longOptKey.substr(2); // remove leading "--"
            }

            // ----- 3.1) EXACT MATCH (e.g. "-s", "--speed") -----
            if (arg == shortOpt || arg == longOpt)
            {
                matchedOption = true;

                if (options[j].requiresArgument)
                {
                    // We need to grab the next token as the argument
                    if (i + 1 < argc)
                    {
                        ++i;  // consume next token
                        parsedOptions[longOptKey] = argv[i]; // store the argument
                    }
                    else
                    {
                        std::cerr << "Error: " << arg << " requires a value.\n";
                        return 1; // exit on error
                    }
                }
                else
                {
                    // Option does not take an argument; store empty string
                    parsedOptions[longOptKey] = "";
                }
                break; // done matching
            }

            // ----- 3.2) MATCH WITH "=..."" (e.g. "-s=10", "--speed=10") -----
            // We'll check if the arg starts with "-s=" or "--speed="
            const std::string shortEq = shortOpt + std::string("=");
            const std::string longEq  = longOpt  + std::string("=");

            // Does arg start with "-s="  ?
            if (arg.rfind(shortEq, 0) == 0)
            {
                matchedOption = true;
                if (!options[j].requiresArgument)
                {
                    std::cerr << "Error: " << shortOpt << " does not take an argument.\n";
                    return 1;
                }
                // Take everything after '-s='
                std::string value = arg.substr(shortEq.size());
                if (value.empty())
                {
                    std::cerr << "Error: " << shortOpt << "= requires a non-empty value.\n";
                    return 1;
                }
                parsedOptions[longOptKey] = value;
                break;
            }
            // Does arg start with "--speed=" ?
            else if (arg.rfind(longEq, 0) == 0)
            {
                matchedOption = true;
                if (!options[j].requiresArgument)
                {
                    std::cerr << "Error: " << longOpt << " does not take an argument.\n";
                    return 1;
                }
                // Take everything after '--speed='
                std::string value = arg.substr(longEq.size());
                if (value.empty())
                {
                    std::cerr << "Error: " << longOpt << "= requires a non-empty value.\n";
                    return 1;
                }
                parsedOptions[longOptKey] = value;
                break;
            }
        } // end for over options

        // If we didn't match any known option, treat it as unknown
        if (!matchedOption)
        {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            return 1;
        }
    } // end for over argv[]

    // 4) PRINT USAGE IF "help" WAS FOUND
    if (argc==1 || parsedOptions.find("help") != parsedOptions.end())
    {
        std::cout << "Usage: " << argv[0] << " [OPTIONS]\n\n";
        for (int j = 0; options[j].shortOpt != nullptr; ++j)
        {
            const char* shortOpt = options[j].shortOpt;  // e.g. "-s"
            const char* longOpt  = options[j].longOpt;   // e.g. "--speed"

            std::cout << "  " << shortOpt << ", " << longOpt;
            if (options[j].requiresArgument) {
                std::cout << " <value>";
            }
            std::cout << "\n      " << options[j].description << "\n\n";
        }
        return 2;
    }

    return 0;
}

