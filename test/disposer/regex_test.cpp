#include <iostream>
#include <regex>

int main(const int argc, char* const* argv)
{
    try {
        std::regex re("[12]", std::regex::ECMAScript);
    }
    catch (const std::regex_error& ex) {
        std::cout << "regex_error caught: " << ex.what() << '\n';
        if (ex.code() == std::regex_constants::error_brack)
            std::cout << "The code was error_brack\n";
    }

    std::regex  incl("one");
    std::regex  excl("two");
    std::smatch match;
    std::string str("one");
    if (std::regex_search(str, match, incl))
        std::cout << "Include matches\n";
    else
        std::cout << "Include doesn't match\n";
}
