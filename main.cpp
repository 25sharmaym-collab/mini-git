#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static const fs::path META_DIR = ".minigit";
static const fs::path INDEX_FILE = META_DIR / "index";
static const fs::path HEAD_FILE = META_DIR / "HEAD";
static const fs::path COMMITS_DIR = META_DIR / "commits";

std::string read_text(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read file: " + path.string());
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

void write_text(const fs::path& path, const std::string& content) {
    if (path.has_parent_path()) fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot write file: " + path.string());
    out << content;
}

std::string hash_content(const std::string& data) {
    // FNV-1a 64-bit: small, deterministic, and suitable for this learning project.
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : data) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

bool initialized() {
    return fs::exists(META_DIR) && fs::is_directory(META_DIR);
}

void require_initialized() {
    if (!initialized()) {
        throw std::runtime_error("not a Mini Git repository; run 'minigit init' first");
    }
}

std::vector<std::string> read_index() {
    std::vector<std::string> files;
    if (!fs::exists(INDEX_FILE)) return files;

    std::ifstream in(INDEX_FILE);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) files.push_back(line);
    }
    return files;
}

void write_index(std::vector<std::string> files) {
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());

    std::ostringstream out;
    for (const auto& file : files) out << file << '\n';
    write_text(INDEX_FILE, out.str());
}

void command_init() {
    if (initialized()) {
        std::cout << "Mini Git repository already initialized.\n";
        return;
    }

    fs::create_directories(COMMITS_DIR);
    write_text(HEAD_FILE, "");
    write_text(INDEX_FILE, "");
    std::cout << "Initialized empty Mini Git repository in " << fs::absolute(META_DIR) << "\n";
}

void command_add(int argc, char* argv[]) {
    require_initialized();
    if (argc < 3) throw std::runtime_error("usage: minigit add <file> [file ...]");

    auto index = read_index();

    for (int i = 2; i < argc; ++i) {
        fs::path path = argv[i];
        if (!fs::exists(path) || !fs::is_regular_file(path)) {
            throw std::runtime_error("file not found: " + path.string());
        }
        if (path.string().rfind(META_DIR.string(), 0) == 0) {
            throw std::runtime_error("cannot stage files inside .minigit");
        }

        index.push_back(path.lexically_normal().string());
        std::cout << "staged: " << path.string() << '\n';
    }

    write_index(index);
}

void command_status() {
    require_initialized();
    const auto index = read_index();

    if (index.empty()) {
        std::cout << "No files staged.\n";
        return;
    }

    std::cout << "Changes staged for commit:\n";
    for (const auto& file : index) {
        std::cout << "  staged: " << file << '\n';
    }
}

std::string current_head() {
    if (!fs::exists(HEAD_FILE)) return "";
    return read_text(HEAD_FILE);
}

void command_commit(int argc, char* argv[]) {
    require_initialized();
    if (argc != 3) {
        throw std::runtime_error("usage: minigit commit \\"message\\"");
    }

    const auto index = read_index();
    if (index.empty()) throw std::runtime_error("nothing to commit");

    const std::string message = argv[2];
    const auto timestamp = std::chrono::system_clock::now().time_since_epoch();
    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(timestamp).count();

    std::ostringstream manifest;
    manifest << "message=" << message << '\n';
    manifest << "timestamp=" << seconds << '\n';

    for (const auto& file : index) {
        if (!fs::exists(file) || !fs::is_regular_file(file)) {
            throw std::runtime_error("staged file no longer exists: " + file);
        }
        manifest << file << "=" << hash_content(read_text(file)) << '\n';
    }

    const std::string commit_id = hash_content(manifest.str());
    const fs::path commit_dir = COMMITS_DIR / commit_id;
    fs::create_directories(commit_dir);

    for (const auto& file : index) {
        const fs::path source(file);
        const fs::path destination = commit_dir / source;
        fs::create_directories(destination.parent_path());
        fs::copy_file(source, destination, fs::copy_options::overwrite_existing);
    }

    write_text(commit_dir / "metadata", manifest.str() + "parent=" + current_head() + "\n");
    write_text(HEAD_FILE, commit_id);
    write_text(INDEX_FILE, "");

    std::cout << "[" << commit_id << "] " << message << '\n';
}

void command_log() {
    require_initialized();
    std::string id = current_head();

    if (id.empty()) {
        std::cout << "No commits yet.\n";
        return;
    }

    while (!id.empty()) {
        const fs::path metadata_path = COMMITS_DIR / id / "metadata";
        if (!fs::exists(metadata_path)) break;

        std::ifstream in(metadata_path);
        std::string line;
        std::string message;
        std::string timestamp;
        std::string parent;

        while (std::getline(in, line)) {
            if (line.rfind("message=", 0) == 0) message = line.substr(8);
            else if (line.rfind("timestamp=", 0) == 0) timestamp = line.substr(10);
            else if (line.rfind("parent=", 0) == 0) parent = line.substr(7);
        }

        std::cout << "commit " << id << '\n';
        std::cout << "  timestamp: " << timestamp << '\n';
        std::cout << "  message: " << message << "\n\n";

        id = parent;
    }
}

void print_help() {
    std::cout
        << "Mini Git - educational version control system\n\n"
        << "Commands:\n"
        << "  init                         Initialize repository\n"
        << "  add <file> [file ...]        Stage files\n"
        << "  status                       Show staged files\n"
        << "  commit \"message\"             Create a commit\n"
        << "  log                          Show commit history\n";
}

int main(int argc, char* argv[]) {
    try {
        if (argc < 2) {
            print_help();
            return 0;
        }

        const std::string command = argv[1];

        if (command == "init") command_init();
        else if (command == "add") command_add(argc, argv);
        else if (command == "status") command_status();
        else if (command == "commit") command_commit(argc, argv);
        else if (command == "log") command_log();
        else if (command == "help" || command == "--help") print_help();
        else throw std::runtime_error("unknown command: " + command);

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
