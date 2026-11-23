#include "front.h"

#include <iostream>
#include <string>
#include <vector>

#include "back.h"
#include "error.h"
#include "helper.h"

namespace {

void clear_screen() {
    std::cout << "\033[2J\033[H" << std::flush;
}

void print_logo() {
    std::cout
        << "==========================\n"
        << "  #####   #    #  # ####  \n"
        << "     #   # #   # #  #     \n"
        << "    #   #   #  ##   ####  \n"
        << "   #    #####  # #  #     \n"
        << "  ##### #   #  #  # ####  \n"
        << "==========================\n";
}

std::string join(const std::vector<std::string>& items) {
    if (items.empty())
        return "(none)";

    std::string result;
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) result += ", ";
        result += items[i];
    }
    return result;
}

void show_api_versions() {
    std::vector<std::string> names;
    for (int v : list_api_versions())
        names.push_back(std::to_string(v));

    std::cout << "\nAPI versions: " << join(names) << "\n";
}

void show_projects() {
    std::cout << "\nProjects: " << join(list_projects()) << "\n";
}

int menu() {
    print_logo();
    std::cout
        << "\n0 - exit\n"
        << "1 - create project\n"
        << "2 - make project\n"
        << "\nInput choice: " << std::flush;

    return read_int();
}

void create_project_front() {
    clear_screen();
    print_logo();

    std::cout << "\nInput project name: " << std::flush;
    std::string name = read_line();

    std::cout
        << "\n1 - simple project\n"
        << "2 - library\n"
        << "\nInput choice: " << std::flush;

    ProjectType type;
    switch (read_int()) {
        case 1:  type = ProjectType::Simple;  break;
        case 2:  type = ProjectType::Library; break;
        default: throw ZakeError(ErrorCode::InvalidInput, "unknown project type");
    }

    show_api_versions();
    std::cout << "Input api version: " << std::flush;
    int version = read_int();

    auto root = create_project(name, type, version);
    std::cout << "\nProject created: " << root.string() << "\n";
}

void make_project_front() {
    clear_screen();
    print_logo();
    show_projects();

    std::cout << "\nInput project name: " << std::flush;
    std::string name = read_line();

    auto output = make_project(name);
    std::cout << "\nBuilt: " << output.string() << "\n";
}

bool resolve_choice(int choice) {
    switch (choice) {
        case 0:
            return false;
        case 1:
            create_project_front();
            return true;
        case 2:
            make_project_front();
            return true;
        default:
            throw ZakeError(ErrorCode::InvalidInput, "no such menu item");
    }
}

}

void front_main() {
    for (;;) {
        clear_screen();

        try {
            if (!resolve_choice(menu()))
                return;
        }
        catch (const ZakeError& e) {
            if (e.code() == ErrorCode::InputClosed)
                return;
            std::cerr << "\nError: " << e.what() << "\n";
        }
        catch (const std::exception& e) {
            std::cerr << "\nUnexpected error: " << e.what() << "\n";
        }

        wait_enter();
    }
}
