# fzf-recent-typo

A fast, typo-tolerant fuzzy file finder for recently modified files.

Built with C++ and [rapidfuzz-cpp](https://github.com/rapidfuzz/rapidfuzz-cpp) for edit-distance based matching that handles typos gracefully.

## Features

- **Instant UI**: Shows immediately while files load in background
- **Typo-tolerant**: Uses edit distance matching (WRatio algorithm)
- **Fast**: Pure C++ with SIMD-optimized rapidfuzz
- **Recent files**: Only shows files modified in the last 2 hours
- **Smart exclusions**: Skips cache directories, node_modules, .git, etc.

## Dependencies

- `rapidfuzz-cpp` - Header-only fuzzy matching library
- `ncurses` - Terminal UI
- `fd` - Fast file finder (runtime dependency)
- A **Nerd Font** - For file type icons (e.g., JetBrainsMono Nerd Font)

On Arch Linux:
```bash
sudo pacman -S rapidfuzz-cpp ncurses fd ttf-jetbrains-mono-nerd
```

## Build & Install

```bash
make
make install  # Installs to ~/.local/bin
```

Or specify a custom prefix:
```bash
make install PREFIX=/usr/local
```

## Usage

```bash
fzf-recent-typo
```

- Type to filter files (typo-tolerant matching)
- Arrow keys or Ctrl+N/P to navigate
- Enter to open selected file with `xdg-open`
- Escape to cancel
- Ctrl+U to clear query

## Configuration

Edit the `fd` command in `main.cpp` to customize:
- Time window (`--changed-within 2h`)
- Excluded directories
- Search root directory

## How It Works

1. UI appears immediately
2. `fd` runs in background thread, streaming results
3. Each keystroke triggers fuzzy matching using rapidfuzz's WRatio
4. WRatio combines multiple similarity metrics for typo tolerance
5. Results update in real-time as you type

## Why Not Just Use fzf?

fzf uses subsequence matching (characters must appear in order). This tool uses edit distance, which handles:
- Transpositions: "tset" matches "test"
- Substitutions: "tast" matches "test"
- Missing chars: "tst" matches "test"

For small file sets (~200 files), typo tolerance is worth the slightly higher compute cost.

## License

MIT
