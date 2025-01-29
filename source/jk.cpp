#include <iostream>
#include <string>
#include <map>

struct OptionSpec {
    const char* shortOpt;        // e.g. "-h"
    const char* longOpt;         // e.g. "--help"
    bool requiresArgument;       // Does this option require an argument?
    const char* description;     // For usage/help text
};

int main(int argc, char* argv[])
{
    // 1) DEFINE OUR OPTION SPECIFICATIONS:
    //    Each entry has shortOpt, longOpt, requiresArgument, and a description
    OptionSpec options[] = {
        {"-h", "--help",   false, "Show this help message"},
        {"-s", "--speed",  true,  "Set the speed (requires a value)"},
        {nullptr, nullptr, false, nullptr} // sentinel to mark end
    };

    // 2) STORAGE FOR PARSED VALUES
    bool showHelp = false;
    bool haveSpeed = false;
    std::string speedValue;

    // 3) PARSE THE COMMAND LINE
    //    We'll iterate over argv[], matching each argument against the table.
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        bool matchedOption = false;

        for (int j = 0; options[j].shortOpt != nullptr; ++j)
        {
            const std::string shortOpt = options[j].shortOpt; // e.g. "-h"
            const std::string longOpt  = options[j].longOpt;  // e.g. "--help"

            // ---- MATCH EXACT SHORT/LONG (e.g. "-s" or "--speed") ----
            if (arg == shortOpt || arg == longOpt)
            {
                matchedOption = true;
                // If the option *requires* an argument, try to get it from argv[i+1]
                if (options[j].requiresArgument)
                {
                    // Make sure there *is* a next token
                    if (i + 1 < argc)
                    {
                        ++i; // consume next token
                        std::string param = argv[i];

                        // Example: if it's the speed option, store the parameter
                        if (shortOpt == "-s") {
                            haveSpeed = true;
                            speedValue = param;
                        }
                    }
                    else
                    {
                        std::cerr << "Error: " << arg << " requires a value.\n";
                        return 1;
                    }
                }
                else
                {
                    // No argument needed. For example, `-h` or `--help`.
                    if (shortOpt == "-h") {
                        showHelp = true;
                    }
                }
                break; // done with inner for-loop
            }

            // ---- MATCH "=" FORM (e.g. "-s=10" or "--speed=10") ----
            // We'll construct strings like "-s=" or "--speed=" and check if `arg` starts with that.
            const std::string shortEq = shortOpt + std::string("=");
            const std::string longEq  = longOpt  + std::string("=");

            if (arg.rfind(shortEq, 0) == 0)  // starts with "-s="
            {
                matchedOption = true;
                if (!options[j].requiresArgument) {
                    std::cerr << "Error: " << shortOpt << " does not take an argument.\n";
                    return 1;
                }
                // Extract text after the '='
                std::string param = arg.substr(shortEq.size());
                if (param.empty()) {
                    std::cerr << "Error: " << shortOpt << "= requires a non-empty value.\n";
                    return 1;
                }
                if (shortOpt == "-s") {
                    haveSpeed = true;
                    speedValue = param;
                }
                break;
            }
            else if (arg.rfind(longEq, 0) == 0)  // starts with "--speed="
            {
                matchedOption = true;
                if (!options[j].requiresArgument) {
                    std::cerr << "Error: " << longOpt << " does not take an argument.\n";
                    return 1;
                }
                // Extract text after the '='
                std::string param = arg.substr(longEq.size());
                if (param.empty()) {
                    std::cerr << "Error: " << longOpt << "= requires a non-empty value.\n";
                    return 1;
                }
                if (shortOpt == "-s") {
                    haveSpeed = true;
                    speedValue = param;
                }
                break;
            }
        } // end for (options)

        // If we didn't match anything in the table, it's unknown
        if (!matchedOption) {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            return 1;
        }
    } // end for (argv[])

    // 4) IF HELP WAS REQUESTED, PRINT IT AND EXIT
    if (showHelp)
    {
        std::cout << "Usage: " << argv[0] << " [OPTIONS]\n\n";
        for (int j = 0; options[j].shortOpt != nullptr; ++j)
        {
            std::cout << "  " 
                      << options[j].shortOpt << ", " << options[j].longOpt;
            if (options[j].requiresArgument) {
                std::cout << " <value>";
            }
            std::cout << "\n      " << options[j].description << "\n\n";
        }
        return 0;
    }

    // 5) USE THE PARSED OPTIONS
    if (haveSpeed)
    {
        std::cout << "Speed is set to: " << speedValue << "\n";
    }
    else
    {
        std::cout << "No speed was provided.\n";
    }

    // ... continue your logic ...

    return 0;
}

