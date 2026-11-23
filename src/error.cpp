#include "error.h"

const char* error_title(ErrorCode code) {
    switch (code) {
        case ErrorCode::NotZrnDir:           return "Run in zrn/";
        case ErrorCode::InvalidFolderStruct: return "Invalid zrn/ struct";
        case ErrorCode::InvalidInput:        return "Invalid input";
        case ErrorCode::InputClosed:         return "Input closed";
        case ErrorCode::InvalidProjectName:  return "Invalid project name";
        case ErrorCode::ProjectExists:       return "Project already exists";
        case ErrorCode::ProjectNotFound:     return "Project not found";
        case ErrorCode::ConfigOpenFailed:    return "Cannot open zake.conf";
        case ErrorCode::ConfigInvalid:       return "Invalid zake.conf";
        case ErrorCode::ApiNotFound:         return "API version not found";
        case ErrorCode::DependencyError:     return "Dependency error";
        case ErrorCode::IoError:             return "I/O error";
        case ErrorCode::ProcessError:        return "Cannot run process";
        case ErrorCode::CompileFailed:       return "Compilation failed";
        case ErrorCode::LinkFailed:          return "Linking failed";
    }
    return "Unknown error";
}

ZakeError::ZakeError(ErrorCode code, const std::string& details)
    : std::runtime_error(details.empty()
                             ? std::string(error_title(code))
                             : std::string(error_title(code)) + ": " + details),
      code_(code) {}
