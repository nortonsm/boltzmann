/*
 * disk_sim.cpp — SFML 3 version
 *
 * Demonstrates:
 *  - Bouncing disks with "coin exchange" collisions
 *  - Real-time chart of fraction of disks vs. coin count
 *  - Overlap fix to prevent orbiting
 *  - Velocity multiplier for faster movement
 *  - Chart scaled 0..0.5, with 0.1 tick marks
 */

#include <SFML/Graphics.hpp>
#include <optional>
#include <random>
#include <cmath>
#include <vector>
#include <string>
#include <iostream>

// ---------------------
// GLOBAL CONSTANTS
// ---------------------
static const int   WIDTH  = 800;
static const int   HEIGHT = 600;
static const int   FPS    = 60;

static const int   DISK_RADIUS        = 40;
static const int   DISK_COUNT         = 6;
static const int   MAX_COINS_PER_DISK = 8;

// Chart region: bottom 200px
static const float CHART_TOP    = 400.f;
static const float CHART_HEIGHT = 200.f;

// A factor to scale all disk velocities
static const float SPEED_MULTIPLIER = 5.0f; // Adjust as desired

// ---------------------
// GLOBAL VARIABLES
// ---------------------
static int collision_count = 0;  // track total collisions

// For chart data: index 0..8 for coin counts
static std::vector<float> xdata[9];
static std::vector<float> ydata[9];
static std::vector<int>   cumulative_counts(9, 0);

// We'll load one global font so it can be used in chart ticks + text
static sf::Font g_font;

// Disk struct
struct Disk {
    float x, y;
    float vx, vy;
    int   radius;
    int   coin_count;
};

// Distance utility
float distance(Disk &a, Disk &b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return std::sqrt(dx*dx + dy*dy);
}

// Collision + coin exchange
bool handle_disk_collision(Disk &d1, Disk &d2, std::mt19937 &rng) {
    float dist = distance(d1, d2);
    if (dist < d1.radius + d2.radius) {
        float nx = (d2.x - d1.x) / dist;
        float ny = (d2.y - d1.y) / dist;
        float v1n = d1.vx * nx + d1.vy * ny;
        float v2n = d2.vx * nx + d2.vy * ny;

        // Simple elastic velocity swap
        d1.vx += (v2n - v1n) * nx;
        d1.vy += (v2n - v1n) * ny;
        d2.vx += (v1n - v2n) * nx;
        d2.vy += (v1n - v2n) * ny;

        // Coin exchange
        std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
        int total_coins_d1 = d1.coin_count;
        int total_coins_d2 = d2.coin_count;

        // d1 -> d2
        int coins_moving_to_d2 = 0;
        for (int i = 0; i < total_coins_d1; i++) {
            if (dist01(rng) < 0.5f) {
                coins_moving_to_d2++;
            }
        }
        d1.coin_count -= coins_moving_to_d2;
        d2.coin_count += coins_moving_to_d2;

        // d2 -> d1
        int coins_moving_to_d1 = 0;
        for (int i = 0; i < total_coins_d2; i++) {
            if (dist01(rng) < 0.5f) {
                coins_moving_to_d1++;
            }
        }
        d2.coin_count -= coins_moving_to_d1;
        d1.coin_count += coins_moving_to_d1;

        // Clamp
        if (d1.coin_count > MAX_COINS_PER_DISK) d1.coin_count = MAX_COINS_PER_DISK;
        if (d2.coin_count > MAX_COINS_PER_DISK) d2.coin_count = MAX_COINS_PER_DISK;

        // Overlap fix: separate them so they don't embed/orbit
        float overlap = (d1.radius + d2.radius) - dist;
        if (overlap > 0.f) {
            float half = overlap * 0.5f;
            d1.x -= nx * half;
            d1.y -= ny * half;
            d2.x += nx * half;
            d2.y += ny * half;
        }

        return true;
    }
    return false;
}

// Move + bounce off edges (top region only)
void update_position(Disk &disk, float dt) {
    disk.x += disk.vx * dt;
    disk.y += disk.vy * dt;

    // bounce left/right
    if (disk.x - disk.radius < 0) {
        disk.x = disk.radius;
        disk.vx = -disk.vx;
    } else if (disk.x + disk.radius > WIDTH) {
        disk.x = WIDTH - disk.radius;
        disk.vx = -disk.vx;
    }

    // bounce top/bottom (CHART_TOP is bottom boundary for the disks)
    if (disk.y - disk.radius < 0) {
        disk.y = disk.radius;
        disk.vy = -disk.vy;
    } else if (disk.y + disk.radius > CHART_TOP) {
        disk.y = CHART_TOP - disk.radius;
        disk.vy = -disk.vy;
    }
}

// Update the plot data
void update_plot(const std::vector<Disk> &disks) {
    // how many disks have each coin count
    std::vector<int> counts(9, 0);
    for (auto &d : disks) {
        counts[d.coin_count]++;
    }

    // update global cumulative counts
    for (int i = 0; i < 9; i++) {
        cumulative_counts[i] += counts[i];
    }

    // push back fraction
    for (int i = 0; i < 9; i++) {
        xdata[i].push_back(static_cast<float>(collision_count));
        float fraction = 0.f;
        if (collision_count > 0) {
            fraction = static_cast<float>(cumulative_counts[i]) / (DISK_COUNT * collision_count);
        }
        ydata[i].push_back(fraction);
    }
}

// Draw the line graph (bottom 200px)
void draw_line_graph(sf::RenderWindow &window) {
    if (collision_count < 1) {
        return; // no data yet
    }

    float chartX     = 0.f;
    float chartY     = CHART_TOP;
    float chartWidth = (float)WIDTH;
    float chartHt    = CHART_HEIGHT;

    // X-axis
    sf::RectangleShape xAxis(sf::Vector2f(chartWidth, 1.f));
    xAxis.setPosition(sf::Vector2f(chartX, chartY + chartHt - 1.f));
    xAxis.setFillColor(sf::Color::White);
    window.draw(xAxis);

    // Y-axis
    sf::RectangleShape yAxis(sf::Vector2f(1.f, chartHt));
    yAxis.setPosition(sf::Vector2f(chartX, chartY));
    yAxis.setFillColor(sf::Color::White);
    window.draw(yAxis);

    // Lambda to scale fraction 0..0.5 to chart height
    auto scaleY = [&](float frac) {
        if (frac > 0.5f) frac = 0.5f; // clamp top
        float proportion = frac / 0.5f; // 0..1
        return chartY + chartHt - (proportion * chartHt);
    };

    // Tick marks every 0.1 up to 0.5
    for (float val = 0.f; val <= 0.5f + 0.0001f; val += 0.1f) {
        float yPos = scaleY(val);

        // a short 5px horizontal line
        sf::RectangleShape tick(sf::Vector2f(5.f, 1.f));
        tick.setFillColor(sf::Color::White);
        tick.setPosition(sf::Vector2f(chartX - 2.f, yPos));
        window.draw(tick);

        // numeric label
        sf::Text label(g_font, std::to_string(val), 12);
        auto lb = label.getLocalBounds();
        // shift so label's right edge is near the axis
        label.setOrigin(sf::Vector2f(lb.size.x, lb.size.y * 0.5f));
        label.setPosition(sf::Vector2f(chartX + 8.f, yPos));
        label.setFillColor(sf::Color::White);
        window.draw(label);
    }

    // scaleX lambda (0..collision_count -> 0..chartWidth)
    auto scaleX = [&](float xVal) {
        if (collision_count == 0) return chartX;
        return chartX + (xVal / float(collision_count)) * chartWidth;
    };

    // 9 lines (coin counts 0..8), each a LineStrip
    sf::Color colors[9] = {
        sf::Color::Blue, sf::Color::Red, sf::Color::Green,
        sf::Color::Cyan, sf::Color::Magenta, sf::Color::Yellow,
        sf::Color::White, sf::Color(128,128,128), sf::Color(255,127,0)
    };

    for (int i = 0; i < 9; i++) {
        sf::VertexArray lineStrip(sf::PrimitiveType::LineStrip);

        for (size_t k = 0; k < xdata[i].size(); k++) {
            float px = scaleX(xdata[i][k]);
            float py = scaleY(ydata[i][k]);

            sf::Vertex v;
            v.position = sf::Vector2f(px, py);
            v.color    = colors[i];

            lineStrip.append(v);
        }
        window.draw(lineStrip);
    }
}

int main() {
    // Random stuff
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<float> velDist(-200.f, 200.f);

    // Create the window (SFML 3 style)
    sf::RenderWindow window(sf::VideoMode({(unsigned)WIDTH, (unsigned)HEIGHT}), "SFML3 Disks + Chart");
    window.setFramerateLimit(FPS);

    // Load global font
    if (!g_font.openFromFile("/System/Library/Fonts/SFNSMono.ttf")) {
        std::cerr << "Failed to open font. Check path!\n";
    }

    // Create disks
    std::vector<Disk> disks(DISK_COUNT);
    std::vector<int> distribution = {8, 0, 0, 0, 0, 0};
    for (int i = 0; i < DISK_COUNT; i++) {
        float x  = (float)(DISK_RADIUS + rand() % (int(CHART_TOP) - 2 * DISK_RADIUS));
        float y  = (float)(DISK_RADIUS + rand() % (int(CHART_TOP) - 2 * DISK_RADIUS));
        float vx = velDist(rng) * SPEED_MULTIPLIER;  // scale velocity
        float vy = velDist(rng) * SPEED_MULTIPLIER;  // scale velocity
        disks[i] = Disk{x, y, vx, vy, DISK_RADIUS, distribution[i]};
    }

    // Time-based throttle
    float time_since_plot = 0.f;
    sf::Clock clock;
    bool running = true;

    while (running && window.isOpen()) {
        float dt = clock.restart().asSeconds();

        // Poll events
        while (auto evOpt = window.pollEvent()) {
            sf::Event e = *evOpt;
            // SFML 3 style: check if e is "Closed"
            if (e.is<sf::Event::Closed>()) {
                window.close();
                running = false;
                break;
            }
        }

        // Update positions
        for (auto &d : disks) {
            update_position(d, dt);
        }

        // Collisions
        int collisions_this_frame = 0;
        for (int i = 0; i < DISK_COUNT; i++) {
            for (int j = i + 1; j < DISK_COUNT; j++) {
                if (handle_disk_collision(disks[i], disks[j], rng)) {
                    collisions_this_frame++;
                }
            }
        }
        collision_count += collisions_this_frame;

        // Chart update every 0.1s if collisions happened
        time_since_plot += dt;
        if (time_since_plot >= 0.1f && collision_count > 0) {
            update_plot(disks);
            time_since_plot = 0.f;
        }

        // Render
        window.clear(sf::Color::Black);

        // Draw disks
        for (auto &d : disks) {
            // Circle
            sf::CircleShape circle(d.radius);
            circle.setFillColor(sf::Color(0,128,255));
            circle.setPosition(sf::Vector2f(d.x - d.radius, d.y - d.radius));
            window.draw(circle);

            // Coin count (requires direct constructor in SFML 3)
            sf::Text text(g_font, std::to_string(d.coin_count), 24);
            text.setFillColor(sf::Color::White);

            // Center in the disk
            auto bounds = text.getLocalBounds();
            text.setOrigin(sf::Vector2f(bounds.size.x * 0.5f, bounds.size.y * 0.5f));
            text.setPosition(sf::Vector2f(d.x, d.y));
            window.draw(text);
        }

        // Draw chart in bottom region
        draw_line_graph(window);

        window.display();
    }

    return 0;
}
