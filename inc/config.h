#pragma once

#include <string>
#include <vector>

enum class ProjectType { Simple, Library };

struct ProjectConfig {
    std::string name;
    ProjectType type = ProjectType::Simple;
    int version = 0;
    std::string output;
    std::vector<std::string> dependencies;
};

bool is_valid_name(const std::string& name);

const char* project_type_name(ProjectType type);

ProjectConfig parse_conf(const std::string& proj_name);
