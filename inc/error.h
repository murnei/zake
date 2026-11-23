#pragma once

#include <stdexcept>
#include <string>

enum class ErrorCode {
    NotZrnDir,
    InvalidFolderStruct,
    InvalidInput,
    InputClosed,
    InvalidProjectName,
    ProjectExists,
    ProjectNotFound,
    ConfigOpenFailed,
    ConfigInvalid,
    ApiNotFound,
    DependencyError,
    IoError,
    ProcessError,
    CompileFailed,
    LinkFailed,
};

const char* error_title(ErrorCode code);

class ZakeError : public std::runtime_error {
public:
    explicit ZakeError(ErrorCode code, const std::string& details = {});

    ErrorCode code() const noexcept { return code_; }

private:
    ErrorCode code_;
};
