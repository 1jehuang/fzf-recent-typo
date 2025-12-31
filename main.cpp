#include <algorithm>
#include <atomic>
#include <clocale>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <rapidfuzz/fuzz.hpp>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

using namespace ftxui;

// Global state
std::vector<std::string> g_files;
std::mutex g_files_mutex;
std::atomic<bool> g_loading{true};

void load_recent_files() {
    FILE* pipe = popen(
        "fd . ~ --type f --changed-within 1w --hidden "
        "--exclude .git --exclude node_modules "
        "--exclude .cache --exclude .local/share "
        "--exclude .config/google-chrome --exclude .mozilla",
        "r"
    );

    if (!pipe) {
        g_loading = false;
        return;
    }

    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        std::string line(buffer);
        if (!line.empty() && line.back() == '\n') {
            line.pop_back();
        }
        if (!line.empty()) {
            std::lock_guard<std::mutex> lock(g_files_mutex);
            g_files.push_back(line);
        }
    }
    pclose(pipe);
    g_loading = false;
}

std::vector<std::string> get_files_snapshot() {
    std::lock_guard<std::mutex> lock(g_files_mutex);
    return g_files;
}

std::string basename(const std::string& path) {
    size_t pos = path.rfind('/');
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

std::string dirname(const std::string& path) {
    size_t pos = path.rfind('/');
    return (pos == std::string::npos) ? "" : path.substr(0, pos + 1);
}

std::string get_extension(const std::string& filename) {
    size_t pos = filename.rfind('.');
    return (pos == std::string::npos || pos == 0) ? "" : filename.substr(pos + 1);
}

// Nerd Font icons for common file types
std::string get_icon(const std::string& filename) {
    std::string ext = get_extension(filename);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // Documents
    if (ext == "pdf") return "";
    if (ext == "doc" || ext == "docx") return "󰈬";
    if (ext == "tex") return "";
    if (ext == "md") return "";
    if (ext == "txt") return "";

    // Code
    if (ext == "cpp" || ext == "cc" || ext == "cxx") return "";
    if (ext == "c" || ext == "h") return "";
    if (ext == "py") return "";
    if (ext == "js") return "";
    if (ext == "ts") return "";
    if (ext == "rs") return "";
    if (ext == "go") return "";
    if (ext == "java") return "";
    if (ext == "rb") return "";
    if (ext == "lua") return "";
    if (ext == "sh" || ext == "bash" || ext == "zsh" || ext == "fish") return "";
    if (ext == "html") return "";
    if (ext == "css") return "";
    if (ext == "json") return "";
    if (ext == "yaml" || ext == "yml") return "";
    if (ext == "toml") return "";
    if (ext == "xml") return "󰗀";

    // Images
    if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "gif" || ext == "bmp" || ext == "svg") return "";

    // Media
    if (ext == "mp3" || ext == "wav" || ext == "flac" || ext == "ogg") return "";
    if (ext == "mp4" || ext == "mkv" || ext == "avi" || ext == "mov") return "";

    // Archives
    if (ext == "zip" || ext == "tar" || ext == "gz" || ext == "xz" || ext == "7z" || ext == "rar") return "";

    // Config/dotfiles
    if (!filename.empty() && filename[0] == '.') return "";

    // Default
    return "";
}

std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::vector<std::pair<std::string, double>> fuzzy_match(
    const std::string& query,
    const std::vector<std::string>& files,
    size_t limit = 50
) {
    std::vector<std::pair<std::string, double>> results;

    if (query.empty()) {
        for (size_t i = 0; i < std::min(limit, files.size()); ++i) {
            results.emplace_back(files[i], 100.0);
        }
        return results;
    }

    std::string query_lower = to_lower(query);

    for (const auto& file : files) {
        std::string base = to_lower(basename(file));
        double score = rapidfuzz::fuzz::WRatio(query_lower, base);
        if (score > 35.0) {
            results.emplace_back(file, score);
        }
    }

    std::sort(results.begin(), results.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    if (results.size() > limit) {
        results.resize(limit);
    }

    return results;
}

std::vector<std::string> read_file_preview(const std::string& path, int max_lines = 20) {
    std::vector<std::string> lines;
    std::ifstream file(path);
    if (!file.is_open()) {
        lines.push_back("[Cannot read file]");
        return lines;
    }

    std::string line;
    int count = 0;
    while (std::getline(file, line) && count < max_lines) {
        // Truncate long lines
        if (line.length() > 80) {
            line = line.substr(0, 77) + "...";
        }
        // Replace tabs with spaces
        size_t pos;
        while ((pos = line.find('\t')) != std::string::npos) {
            line.replace(pos, 1, "  ");
        }
        lines.push_back(line);
        count++;
    }

    if (lines.empty()) {
        lines.push_back("[Empty file]");
    }

    return lines;
}

int main() {
    setlocale(LC_ALL, "");

    // Start loading files in background
    std::thread loader(load_recent_files);

    auto screen = ScreenInteractive::Fullscreen();

    std::string query;
    int selected = 0;
    std::string chosen;
    std::vector<std::pair<std::string, double>> matches;

    // Input component for the query
    auto input_option = InputOption();
    input_option.on_change = [&] {
        selected = 0;  // Reset selection when query changes
    };
    auto input = Input(&query, "Type to search...", input_option);

    // Main component
    auto component = CatchEvent(input, [&](Event event) {
        auto files = get_files_snapshot();
        matches = fuzzy_match(query, files);

        // Handle navigation
        if (event == Event::ArrowDown || event == Event::CtrlN) {
            if (!matches.empty() && selected < (int)matches.size() - 1) {
                selected++;
            }
            return true;
        }
        if (event == Event::ArrowUp || event == Event::CtrlP) {
            if (selected > 0) {
                selected--;
            }
            return true;
        }

        // Ctrl+U to clear
        if (event == Event::CtrlU) {
            query.clear();
            selected = 0;
            return true;
        }

        // Ctrl+W to delete word (standard terminal binding)
        if (event == Event::CtrlW) {
            // Delete last word
            while (!query.empty() && query.back() == ' ') query.pop_back();
            while (!query.empty() && query.back() != ' ') query.pop_back();
            selected = 0;
            return true;
        }

        // Enter to select
        if (event == Event::Return) {
            if (!matches.empty() && selected < (int)matches.size()) {
                chosen = matches[selected].first;
            }
            screen.Exit();
            return true;
        }

        // Escape to cancel
        if (event == Event::Escape) {
            screen.Exit();
            return true;
        }

        return false;
    });

    // Renderer
    auto renderer = Renderer(component, [&] {
        auto files = get_files_snapshot();
        matches = fuzzy_match(query, files);

        // Clamp selection
        if (selected >= (int)matches.size()) {
            selected = matches.empty() ? 0 : matches.size() - 1;
        }

        // Build file list
        Elements file_elements;
        int visible_count = std::min((int)matches.size(), 30);

        for (int i = 0; i < visible_count; ++i) {
            const auto& [file, score] = matches[i];
            std::string fname = basename(file);
            std::string dir = dirname(file);
            std::string icon = get_icon(fname);

            Element entry;
            if (i == selected) {
                entry = hbox({
                    text(" " + icon + " ") | color(Color::Cyan),
                    text(fname) | bold | color(Color::White),
                    text(" "),
                    text(dir) | dim | color(Color::Blue),
                }) | bgcolor(Color::RGB(40, 44, 52)) | flex;
            } else {
                entry = hbox({
                    text("  " + icon + " ") | color(Color::Cyan),
                    text(fname) | bold,
                    text(" "),
                    text(dir) | dim | color(Color::GrayDark),
                });
            }
            file_elements.push_back(entry);
        }

        if (file_elements.empty()) {
            file_elements.push_back(text("  No matches") | dim);
        }

        // Build preview pane
        Elements preview_elements;
        if (!matches.empty() && selected < (int)matches.size()) {
            auto preview_lines = read_file_preview(matches[selected].first);
            for (const auto& line : preview_lines) {
                preview_elements.push_back(text(line) | color(Color::GrayLight));
            }
        } else {
            preview_elements.push_back(text("No file selected") | dim);
        }

        // Status line
        std::string status = std::to_string(matches.size()) + " matches";
        if (g_loading) {
            status += " (loading...)";
        }
        status += " | " + std::to_string(files.size()) + " files";

        // Build layout
        auto left_pane = vbox({
            hbox({
                text(" ") | color(Color::Cyan) | bold,
                text(" "),
                component->Render() | flex,
            }) | border,
            vbox(file_elements) | flex | frame,
        }) | flex;

        // Get terminal size for responsive layout
        auto terminal = Terminal::Size();
        bool show_preview = terminal.dimx >= 100;

        if (show_preview) {
            auto right_pane = vbox({
                text(" Preview") | bold | color(Color::Yellow),
                separator(),
                vbox(preview_elements) | flex | frame,
            }) | border | size(WIDTH, EQUAL, 50);

            return vbox({
                hbox({
                    left_pane,
                    right_pane,
                }) | flex,
                text(status) | dim | center,
            });
        } else {
            // Narrow window - no preview
            return vbox({
                left_pane | flex,
                text(status) | dim | center,
            });
        }
    });

    // Refresh periodically while loading
    std::atomic<bool> running{true};
    std::thread refresh_thread([&] {
        while (running && g_loading) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            screen.Post(Event::Custom);
        }
    });

    screen.Loop(renderer);

    running = false;
    if (refresh_thread.joinable()) {
        refresh_thread.join();
    }
    if (loader.joinable()) {
        loader.join();
    }

    if (!chosen.empty()) {
        std::string cmd = "nohup xdg-open \"" + chosen + "\" >/dev/null 2>&1 &";
        system(cmd.c_str());
    }

    return 0;
}
