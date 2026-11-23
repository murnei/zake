#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "config.h"

void check_folder();

std::vector<std::string> list_projects();
std::vector<int> list_api_versions();

std::filesystem::path create_project(const std::string& proj_name,
                                     ProjectType type, int api_ver);

std::filesystem::path make_project(const std::string& proj_name);
