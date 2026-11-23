#include "back.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>

#include "error.h"
#include "process.h"
#include "zrn.h"

namespace fs = std::filesystem;

namespace {

const fs::path kProjectsDir = "projects";
const fs::path kTargetDir   = "target";
const fs::path kApiDir      = "api";
const std::string kApiPrefix = "api_";
const fs::path kLibsDirName = "libs";

const std::vector<std::string> kCFlags = {
    "-m32", "-march=i686", "-O2",
    "-ffreestanding", "-fno-pie", "-fno-stack-protector",
    "-nostdlib", "-nostdinc",
    "-fomit-frame-pointer", "-mregparm=3",
    "-fno-asynchronous-unwind-tables", "-fno-unwind-tables",
};

fs::path project_root(const std::string& name) { return kProjectsDir / name; }

fs::path api_path(int version) {
    return kApiDir / (kApiPrefix + std::to_string(version));
}

class TempDir {
public:
    TempDir() {
        std::string tmpl = (fs::temp_directory_path() / "zake-XXXXXX").string();
        if (!mkdtemp(tmpl.data()))
            throw ZakeError(ErrorCode::IoError, "cannot create temporary directory");
        path_ = tmpl;
    }

    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

std::string c_ident(std::string s) {
    for (char& c : s)
        if (!std::isalnum(static_cast<unsigned char>(c)))
            c = '_';
    if (std::isdigit(static_cast<unsigned char>(s.front())))
        s.insert(0, "_");
    return s;
}

void write_text_file(const fs::path& path, const std::string& text) {
    std::ofstream f(path);
    if (!f.is_open())
        throw ZakeError(ErrorCode::IoError, "cannot create " + path.string());
    f << text;
    f.close();
    if (!f)
        throw ZakeError(ErrorCode::IoError, "failed to write " + path.string());
}

void run_checked(const std::vector<std::string>& cmd, ErrorCode on_fail) {
    ProcessResult r = run_process(cmd);

    if (r.exit_code == 127 && r.output.empty())
        throw ZakeError(ErrorCode::ProcessError,
                        "'" + cmd.front() + "' not found (is it installed?)");

    if (r.exit_code != 0)
        throw ZakeError(on_fail, r.output.empty() ? cmd.front() + " failed" : r.output);
}

void create_simple_template(const fs::path& root, const std::string&) {
    write_text_file(root / "src" / "main.c",
        "#include \"zrn.h\"\n"
        "#include \"stdio.h\"\n"
        "\n"
        "ZRN_ENTRY\n"
        "void main() {\n"
        "    print(\"Hello world!\");\n"
        "    return;\n"
        "}\n");
}

void create_library_template(const fs::path& root, const std::string& name) {
    std::string id = c_ident(name);

    write_text_file(root / "inc" / (name + ".h"),
        "#pragma once\n"
        "\n"
        "int " + id + "_add(int a, int b);\n");

    write_text_file(root / "src" / (name + ".c"),
        "#include \"" + name + ".h\"\n"
        "\n"
        "int " + id + "_add(int a, int b) {\n"
        "    return a + b;\n"
        "}\n");
}



struct Dependencies {
    std::vector<std::string> include_flags;
    std::vector<std::string> archives;
};

void extract_headers(const fs::path& archive, const fs::path& dst) {
    ProcessResult l = run_process({"ar", "t", archive.string()});
    if (l.exit_code != 0)
        throw ZakeError(ErrorCode::DependencyError, "cannot read " + archive.string());

    std::istringstream in(l.output);
    std::string member;
    while (std::getline(in, member)) {
        if (!member.empty() && member.back() == '\r')
            member.pop_back();
        if (member.size() < 3 || member.compare(member.size() - 2, 2, ".h") != 0)
            continue;
        if (member.find('/') != std::string::npos)
            continue;

        ProcessResult r = run_process({"ar", "p", archive.string(), member});
        if (r.exit_code != 0)
            throw ZakeError(ErrorCode::DependencyError,
                            "cannot extract " + member + " from " + archive.string());
        write_text_file(dst / member, r.output);
    }
}

Dependencies resolve_dependencies(const ProjectConfig& cfg, const fs::path& hdr_dir) {
    Dependencies deps;

    const fs::path libs_dir = project_root(cfg.name) / kLibsDirName;
    fs::create_directories(hdr_dir);
    deps.include_flags.push_back("-I" + hdr_dir.string());

    for (const auto& lib_file : cfg.dependencies) {
        if (lib_file == cfg.output)
            throw ZakeError(ErrorCode::DependencyError, "'" + lib_file + "' depends on itself");

        fs::path archive = libs_dir / lib_file;
        if (!fs::is_regular_file(archive))
            throw ZakeError(ErrorCode::DependencyError,
                            "library '" + lib_file + "' not found (" + archive.string() + ")");

        extract_headers(archive, hdr_dir);
        deps.archives.push_back(archive.string());
    }
    return deps;
}

std::vector<fs::path> collect_sources(const fs::path& src_dir) {
    if (!fs::is_directory(src_dir))
        throw ZakeError(ErrorCode::CompileFailed, "no directory " + src_dir.string());

    std::vector<fs::path> sources;
    for (const auto& e : fs::recursive_directory_iterator(src_dir)) {
        if (e.is_regular_file() && e.path().extension() == ".c")
            sources.push_back(e.path());
    }

    if (sources.empty())
        throw ZakeError(ErrorCode::CompileFailed, "no .c files in " + src_dir.string());

    std::sort(sources.begin(), sources.end());
    return sources;
}

fs::path object_path(const fs::path& build_dir, const fs::path& src_dir,
                     const fs::path& source) {
    std::string flat = fs::relative(source, src_dir).generic_string();
    std::replace(flat.begin(), flat.end(), '/', '_');
    return build_dir / (flat + ".o");
}

fs::path compile_source(const ProjectConfig& cfg, const fs::path& source,
                        const fs::path& object, const Dependencies& deps) {
    std::vector<std::string> cmd = {"gcc"};
    cmd.insert(cmd.end(), kCFlags.begin(), kCFlags.end());

    cmd.push_back("-I" + (project_root(cfg.name) / "inc").string());
    cmd.push_back("-I" + (api_path(cfg.version) / "inc").string());
    cmd.insert(cmd.end(), deps.include_flags.begin(), deps.include_flags.end());

    cmd.insert(cmd.end(), {"-c", source.string(), "-o", object.string()});

    run_checked(cmd, ErrorCode::CompileFailed);
    return object;
}

void link_library(const std::vector<fs::path>& objects, const fs::path& out,
                  const fs::path& inc_dir) {
    fs::create_directories(out.parent_path());
    fs::remove(out);

    std::vector<std::string> cmd = {"ar", "rcs", out.string()};
    for (const auto& o : objects)
        cmd.push_back(o.string());

    if (fs::is_directory(inc_dir)) {
        std::vector<fs::path> headers;
        for (const auto& e : fs::directory_iterator(inc_dir))
            if (e.is_regular_file() && e.path().extension() == ".h")
                headers.push_back(e.path());
        std::sort(headers.begin(), headers.end());
        for (const auto& h : headers)
            cmd.push_back(h.string());
    }

    run_checked(cmd, ErrorCode::LinkFailed);
}

fs::path find_linker_script(int api_version) {
    for (const fs::path& p : {api_path(api_version) / "zrn.ld", fs::path("zrn.ld")}) {
        if (fs::is_regular_file(p))
            return p;
    }
    throw ZakeError(ErrorCode::LinkFailed,
                    "zrn.ld not found (looked in " + api_path(api_version).string() +
                    "/ and zrn/)");
}

void link_program(const ProjectConfig& cfg, const std::vector<fs::path>& objects,
                  const Dependencies& deps, const fs::path& build_dir,
                  const fs::path& out) {
    fs::path script = find_linker_script(cfg.version);
    fs::path flat   = build_dir / (cfg.name + ".bin");

    std::vector<std::string> cmd = {"ld", "-m", "elf_i386", "--oformat", "binary",
                                    "-T", script.string(), "-o", flat.string()};

    for (const auto& o : objects)
        cmd.push_back(o.string());

    cmd.insert(cmd.end(), deps.archives.begin(), deps.archives.end());

    fs::path api_archive = api_path(cfg.version) / "libapi.a";
    cmd.push_back(api_archive.string());

    run_checked(cmd, ErrorCode::LinkFailed);

    write_zrn(flat, out, static_cast<uint32_t>(cfg.version));
}

}

void check_folder() {
    fs::path current = fs::current_path();

    if (current.filename() != "zrn")
        throw ZakeError(ErrorCode::NotZrnDir);

    for (const fs::path& dir : {kProjectsDir, kTargetDir, kApiDir}) {
        if (!fs::is_directory(current / dir))
            throw ZakeError(ErrorCode::InvalidFolderStruct, "missing " + dir.string() + "/");
    }
}

std::vector<std::string> list_projects() {
    std::vector<std::string> result;
    for (const auto& e : fs::directory_iterator(kProjectsDir)) {
        if (e.is_directory())
            result.push_back(e.path().filename().string());
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<int> list_api_versions() {
    std::vector<int> result;
    for (const auto& e : fs::directory_iterator(kApiDir)) {
        if (!e.is_directory())
            continue;

        std::string name = e.path().filename().string();
        if (name.compare(0, kApiPrefix.size(), kApiPrefix) != 0)
            continue;

        std::string num = name.substr(kApiPrefix.size());
        if (num.empty() || !std::all_of(num.begin(), num.end(),
                [](unsigned char c) { return std::isdigit(c); }))
            continue;

        try {
            result.push_back(std::stoi(num));
        } catch (...) {  }
    }
    std::sort(result.begin(), result.end());
    return result;
}

fs::path create_project(const std::string& proj_name, ProjectType type, int api_ver) {
    if (!is_valid_name(proj_name))
        throw ZakeError(ErrorCode::InvalidProjectName,
                        "'" + proj_name + "' (allowed: letters, digits, '_' and '-')");

    if (!fs::is_directory(api_path(api_ver)))
        throw ZakeError(ErrorCode::ApiNotFound, std::to_string(api_ver));

    fs::path root = project_root(proj_name);
    if (fs::exists(root))
        throw ZakeError(ErrorCode::ProjectExists, proj_name);

    try {
        fs::create_directories(root / "src");
        fs::create_directories(root / "inc");
        fs::create_directories(root / kLibsDirName);

        if (type == ProjectType::Library)
            create_library_template(root, proj_name);
        else
            create_simple_template(root, proj_name);

        std::string output = (type == ProjectType::Library)
                                 ? "lib" + proj_name + ".a"
                                 : proj_name + ".zrn";

        write_text_file(root / "zake.conf",
            "[project]\n"
            "name=" + proj_name + "\n"
            "version=" + std::to_string(api_ver) + "\n"
            "type=" + project_type_name(type) + "\n"
            "output=" + output + "\n"
            "\n[dependencies]\n");
    } catch (...) {
        std::error_code ec;
        fs::remove_all(root, ec);
        throw;
    }

    return root;
}

fs::path make_project(const std::string& proj_name) {
    ProjectConfig cfg = parse_conf(proj_name);

    if (!fs::is_directory(api_path(cfg.version)))
        throw ZakeError(ErrorCode::ApiNotFound, std::to_string(cfg.version));

    fs::path root      = project_root(cfg.name);
    fs::path src_dir   = root / "src";
    fs::path out       = (cfg.type == ProjectType::Library)
                             ? root / kLibsDirName / cfg.output
                             : kTargetDir / cfg.output;

    TempDir tmp;
    const fs::path& build_dir = tmp.path();

    Dependencies deps = resolve_dependencies(cfg, build_dir / "deps");
    std::vector<fs::path> sources = collect_sources(src_dir);

    std::vector<fs::path> objects;
    for (const auto& source : sources)
        objects.push_back(compile_source(cfg, source,
                                         object_path(build_dir, src_dir, source), deps));

    if (cfg.type == ProjectType::Library)
        link_library(objects, out, root / "inc");
    else
        link_program(cfg, objects, deps, build_dir, out);

    return out;
}
