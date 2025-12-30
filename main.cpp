#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <ncurses.h>
#include <rapidfuzz/fuzz.hpp>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

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

std::string to_lower(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::vector<std::pair<std::string, double>> fuzzy_match(
    const std::string& query,
    const std::vector<std::string>& files,
    size_t limit = 30
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

int main() {
    // Start loading files in background thread
    std::thread loader(load_recent_files);

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);

    std::string query;
    size_t selected = 0;
    std::string chosen;

    while (true) {
        clear();
        int height, width;
        getmaxyx(stdscr, height, width);

        auto files = get_files_snapshot();
        auto matches = fuzzy_match(query, files, height - 3);

        // Draw prompt with loading indicator
        if (g_loading) {
            mvprintw(0, 0, "> %s (loading...)", query.c_str());
        } else {
            mvprintw(0, 0, "> %s", query.c_str());
        }

        // Draw matches
        for (size_t i = 0; i < matches.size() && (int)(i + 2) < height; ++i) {
            const auto& [file, score] = matches[i];
            std::string display = file;
            if ((int)display.length() > width - 4) {
                display = display.substr(display.length() - width + 4);
            }

            if (i == selected) {
                attron(A_REVERSE);
                mvprintw(i + 2, 0, "▶ %s", display.c_str());
                attroff(A_REVERSE);
            } else {
                mvprintw(i + 2, 0, "  %s", display.c_str());
            }
        }

        move(0, 2 + query.length());
        refresh();

        // Non-blocking input with timeout for responsive loading updates
        timeout(g_loading ? 50 : -1);
        int ch = getch();

        if (ch == ERR) continue; // Timeout, just refresh

        if (ch == 27) { // Escape
            break;
        } else if (ch == '\n' || ch == '\r') {
            if (!matches.empty() && selected < matches.size()) {
                chosen = matches[selected].first;
            }
            break;
        } else if (ch == KEY_UP || ch == 16) { // Up or Ctrl+P
            if (selected > 0) selected--;
        } else if (ch == KEY_DOWN || ch == 14) { // Down or Ctrl+N
            if (!matches.empty() && selected < matches.size() - 1) selected++;
        } else if (ch == KEY_BACKSPACE || ch == 127) {
            if (!query.empty()) {
                query.pop_back();
                selected = 0;
            }
        } else if (ch == 21) { // Ctrl+U
            query.clear();
            selected = 0;
        } else if (ch >= 32 && ch <= 126) {
            query += (char)ch;
            selected = 0;
        }
    }

    endwin();

    // Wait for loader thread to finish
    if (loader.joinable()) {
        loader.join();
    }

    if (!chosen.empty()) {
        std::string cmd = "nohup xdg-open \"" + chosen + "\" >/dev/null 2>&1 &";
        system(cmd.c_str());
    }

    return 0;
}
