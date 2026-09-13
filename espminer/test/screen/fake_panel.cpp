#include "fake_panel.h"
#include <cstdio>
#include <cstring>

Panel panel;

void Panel::reset() {
    ops.clear();
    memset(occupancy, ' ', sizeof(occupancy));
}

void Panel::add(const DrawOp &op) {
    ops.push_back(op);
    char mark = op.isText ? 'T' : 'G';
    for (int yy = op.y; yy < op.y + op.h; yy++) {
        for (int xx = op.x; xx < op.x + op.w; xx++) {
            if (yy < 0 || yy >= H || xx < 0 || xx >= W) continue;
            if (occupancy[yy][xx] == 'T' && mark == 'G') occupancy[yy][xx] = 'X';
            else if (occupancy[yy][xx] == ' ') occupancy[yy][xx] = mark;
        }
    }
}

int Panel::outOfBounds(std::vector<std::string> &why) const {
    int bad = 0;
    for (const DrawOp &op : ops) {
        if (op.x >= 0 && op.y >= 0 && op.x + op.w <= W && op.y + op.h <= H) continue;
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "%s '%s' at (%d,%d) size %dx%d -> right edge %d, bottom edge %d",
                 op.kind.c_str(), op.text.c_str(), op.x, op.y, op.w, op.h,
                 op.x + op.w, op.y + op.h);
        why.push_back(buf);
        bad++;
    }
    return bad;
}

int Panel::textCollisions(std::vector<std::string> &why) const {
    int hits = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (occupancy[y][x] == 'X') hits++;
    if (hits) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%d pixel(s) where graphics are drawn over text", hits);
        why.push_back(buf);
    }
    return hits;
}

// Print the panel at character resolution: one cell per 6x8 glyph box, so
// text reads normally and graphics show up as blocks.
void Panel::render(const char *title) const {
    const int cols = W / 6, rows = H / 8;
    std::vector<std::string> grid(rows, std::string(cols, '.'));

    for (const DrawOp &op : ops) {
        if (op.isText) {
            int row = op.y / 8, col = op.x / 6;
            for (size_t i = 0; i < op.text.size(); i++) {
                int c = col + (int)i * op.size;
                if (row < 0 || row >= rows || c < 0 || c >= cols) continue;
                grid[row][c] = op.text[i];
                if (op.size > 1 && c + 1 < cols) grid[row][c + 1] = '_';
            }
            if (op.size > 1 && row + 1 < rows)
                for (int c = col; c < col + (int)op.text.size() * op.size && c < cols; c++)
                    if (grid[row + 1][c] == '.') grid[row + 1][c] = '_';
        } else {
            for (int yy = op.y; yy < op.y + op.h; yy++)
                for (int xx = op.x; xx < op.x + op.w; xx++) {
                    int r = yy / 8, c = xx / 6;
                    if (r < 0 || r >= rows || c < 0 || c >= cols) continue;
                    if (grid[r][c] == '.') grid[r][c] = '#';
                }
        }
    }

    printf("\n  %s\n  +", title);
    for (int i = 0; i < cols; i++) printf("-");
    printf("+\n");
    for (int r = 0; r < rows; r++) printf("  |%s|\n", grid[r].c_str());
    printf("  +");
    for (int i = 0; i < cols; i++) printf("-");
    printf("+\n");
}
