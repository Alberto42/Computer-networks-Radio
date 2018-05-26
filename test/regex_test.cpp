// regex_match example
#include <iostream>
#include <string>
#include <regex>

static const char *const string = "BOREWICZ_HERE 233.77.999.4 20657 Nazwa";
std::regex regex("BOREWICZ_HERE (([0-9]{1,3}\\.){3}[0-9]{1,3}) ([0-9]{1,5}) ([a-zA-Z0-9_ ]{1,64})");
int main ()
{
    std::cmatch cm;

    // using explicit flags:
    std::regex_match ( string, cm, regex);

    std::cout << "the matches were: ";
    for (unsigned i=0; i<cm.size(); ++i) {
        std::cout << "[" << cm[i] << "] ";
    }

    std::cout << std::endl;

    return 0;
}