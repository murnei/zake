#include "config.h"

#include <cctype>
#include <filesystem>
#include <fstream>

#include "error.h"

namespace fs = std::filesystem;

namespace {

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    size_t b = s.find_first_not_of(ws);
    if (b == std::string::npos)
        return {};
    size_t e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

[[noreturn]] void bad_line(int line_no, const std::string& msg) {
    throw ZakeError(ErrorCode::ConfigInvalid,
                    "line " + std::to_string(line_no) + ": " + msg);
}

ProjectType parse_type(const std::string& value, int line_no) {
    if (value == "simple")  return ProjectType::Simple;
    if (value == "library") return ProjectType::Library;
    bad_line(line_no, "unknown project type '" + value + "'");
}

int parse_version(const std::string& value, int line_no) {
    size_t pos = 0;
    int v = 0;
    try {
        v = std::stoi(value, &pos);
    } catch (...) {
        pos = 0;
    }
    if (pos != value.size() || v <= 0)
        bad_line(line_no, "version must be a positive integer");
    return v;
}

void validate(const ProjectConfig& cfg) {
    auto missing = [](const char* key) {
        throw ZakeError(ErrorCode::ConfigInvalid,
                        std::string("missing required key '") + key + "'");
    };

    if (cfg.name.empty())   missing("name");
    if (cfg.version == 0)   missing("version");
    if (cfg.output.empty()) missing("output");

    if (!is_valid_name(cfg.name))
        throw ZakeError(ErrorCode::ConfigInvalid, "bad project name '" + cfg.name + "'");

    if (fs::path(cfg.output).filename() != fs::path(cfg.output))
        throw ZakeError(ErrorCode::ConfigInvalid, "output must be a file name, not a path");

    const char* ext = (cfg.type == ProjectType::Library) ? ".a" : ".zrn";
    if (!ends_with(cfg.output, ext))
        throw ZakeError(ErrorCode::ConfigInvalid,
                        std::string(project_type_name(cfg.type)) +
                            " output must end with '" + ext + "'");
}

}

bool is_valid_name(const std::string& name) {
    if (name.empty() || name.size() > 64 || name.front() == '-')
        return false;

    for (unsigned char c : name) {
        if (!std::isalnum(c) && c != '_' && c != '-')
            return false;
    }
    return true;
}

bool is_valid_lib_file(const std::string& s) {
    if (s.empty() || s == "." || s == "..")
        return false;
    for (unsigned char c : s) {
        if (!std::isalnum(c) && c != '_' && c != '-' && c != '.')
            return false;
    }
    return true;
}

const char* project_type_name(ProjectType type) {
    return type == ProjectType::Library ? "library" : "simple";
}

ProjectConfig parse_conf(const std::string& proj_name) {
    if (!is_valid_name(proj_name))
        throw ZakeError(ErrorCode::InvalidProjectName, "'" + proj_name + "'");

    fs::path root = fs::path("projects") / proj_name;
    if (!fs::is_directory(root))
        throw ZakeError(ErrorCode::ProjectNotFound, proj_name);

    fs::path conf_path = root / "zake.conf";
    std::ifstream conf(conf_path);
    if (!conf.is_open())
        throw ZakeError(ErrorCode::ConfigOpenFailed, conf_path.string());

    ProjectConfig cfg;
    std::string raw, section;
    int line_no = 0;

    while (std::getline(conf, raw)) {
        ++line_no;
        std::string line = trim(raw);

        if (line.empty() || line.front() == '#')
            continue;

        if (line.front() == '[') {
            if (line.back() != ']')
                bad_line(line_no, "unterminated section header");
            section = trim(line.substr(1, line.size() - 2));
            if (section != "project" && section != "dependencies")
                bad_line(line_no, "unknown section [" + section + "]");
            continue;
        }

        if (section == "project") {
            size_t pos = line.find('=');
            if (pos == std::string::npos)
                bad_line(line_no, "expected key=value");

            std::string key = trim(line.substr(0, pos));
            std::string value = trim(line.substr(pos + 1));

            if (key == "name")         cfg.name = value;
            else if (key == "version") cfg.version = parse_version(value, line_no);
            else if (key == "type")    cfg.type = parse_type(value, line_no);
            else if (key == "output")  cfg.output = value;
            else bad_line(line_no, "unknown key '" + key + "'");
        }
        else if (section == "dependencies") {
            if (!is_valid_lib_file(line))
                bad_line(line_no, "bad dependency name '" + line + "'");
            cfg.dependencies.push_back(line);
        }
        else {
            bad_line(line_no, "entry outside of a section");
        }
    }

    validate(cfg);
    return cfg;
}
