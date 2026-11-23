#include <exception>
#include <iostream>

#include "back.h"
#include "error.h"
#include "front.h"

static void clear_screen() {
    std::cout << "\033[2J\033[H" << std::flush;
}

int main() {
    try {
        check_folder();
        front_main();
    }
    catch (const ZakeError& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }

    clear_screen();

    return 0;
}
