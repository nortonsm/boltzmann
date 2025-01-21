/*
 * disk_sim.cpp (SFML 3)
 * Modified version with:
 * - Non-overlapping disk initialization
 * - Support for larger number of disks and coins
 * - Reduced disk size
 */

#include <SFML/Graphics.hpp>
#include <optional>
#include <random>
#include <cmath>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>

// ---------------------
// GLOBAL CONSTANTS
// ---------------------
static const int WIDTH  = 800;
static const int HEIGHT = 600;
static const int FPS    = 60;

// Reduced disk radius by 50%
static const int DISK_RADIUS         = 20;  // Was 40
static const int DISK_COUNT          = 20;  // Increased from 6
static const int MAX_COINS_PER_DISK  = 25;  // Increased from 8

static const float CHART_TOP    = 400.f;
static const float CHART_HEIGHT = 200.f;

static float g_speedFactor = 5.0f;

// ---------------------
// GLOBALS FOR CHART
// ---------------------
static int collision_count = 0;

// Each coin count (0..25): store x (collision_count) and fraction
static std::vector<float> xdata[26];  // Increased from 9
static std::vector<float> ydata[26];  // Increased from 9
static std::vector<int>   cumulative_counts(26, 0);  // Increased from 9

static float g_coinFraction[26] = {0.f};  // Increased from 9

static sf::Font g_font;

struct Disk {
    float x, y;
    float vx, vy;
    int   radius;
    int   coin_count;
};

// Distance utility
float distance(const Disk &a, const Disk &b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return std::sqrt(dx*dx + dy*dy);
}

// New function: Check if a position is valid (no overlaps)
bool is_valid_position(const std::vector<Disk> &existing_disks, float x, float y, int radius) {
    // Check bounds first
    if (x - radius < 0 || x + radius > WIDTH || 
        y - radius < 0 || y + radius > CHART_TOP) {
        return false;
    }
    
    // Check for overlaps with existing disks
    for (const auto &disk : existing_disks) {
        float dx = x - disk.x;
        float dy = y - disk.y;
        float min_dist = (radius + disk.radius) * 1.1f; // Add 10% margin
        if (dx*dx + dy*dy < min_dist*min_dist) {
            return false;
        }
    }
    return true;
}

// New function: Generate valid random position
std::optional<std::pair<float, float>> find_valid_position(
    const std::vector<Disk> &existing_disks,
    std::mt19937 &rng,
    int radius,
    int max_attempts = 1000
) {
    std::uniform_real_distribution<float> x_dist(radius, WIDTH - radius);
    std::uniform_real_distribution<float> y_dist(radius, CHART_TOP - radius);
    
    for (int i = 0; i < max_attempts; i++) {
        float x = x_dist(rng);
        float y = y_dist(rng);
        if (is_valid_position(existing_disks, x, y, radius)) {
            return std::make_pair(x, y);
        }
    }
    return std::nullopt;
}

bool handle_disk_collision(Disk &d1, Disk &d2, std::mt19937 &rng) {
    float dist = distance(d1, d2);
    if (dist < d1.radius + d2.radius) {
        // --- Elastic collision (equal masses) ---
        float nx = (d2.x - d1.x) / dist;
        float ny = (d2.y - d1.y) / dist;

        float v1n = d1.vx * nx + d1.vy * ny;
        float v2n = d2.vx * nx + d2.vy * ny;

        d1.vx += (v2n - v1n) * nx;
        d1.vy += (v2n - v1n) * ny;
        d2.vx += (v1n - v2n) * nx;
        d2.vy += (v1n - v2n) * ny;

        // --- Uniform Probability Redistribution for coin exchange ---
        int total_coins = d1.coin_count + d2.coin_count;

        std::vector<std::pair<int, int>> possible_redistributions;
        possible_redistributions.reserve(total_coins + 1);

        for (int coins_in_d1 = 0; coins_in_d1 <= total_coins; coins_in_d1++) {
            int coins_in_d2 = total_coins - coins_in_d1;
            if (coins_in_d1 <= MAX_COINS_PER_DISK && coins_in_d2 <= MAX_COINS_PER_DISK) {
                possible_redistributions.emplace_back(coins_in_d1, coins_in_d2);
            }
        }

        if (!possible_redistributions.empty()) {
            std::uniform_int_distribution<size_t> uni_dist(0, possible_redistributions.size() - 1);
            size_t idx = uni_dist(rng);
            d1.coin_count = possible_redistributions[idx].first;
            d2.coin_count = possible_redistributions[idx].second;
        }

        // --- Overlap fix ---
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

void update_position(Disk &disk, float dt) {
    disk.x += disk.vx * dt * g_speedFactor;
    disk.y += disk.vy * dt * g_speedFactor;

    if (disk.x - disk.radius < 0) {
        disk.x = disk.radius;
        disk.vx = -disk.vx;
    } else if (disk.x + disk.radius > WIDTH) {
        disk.x = WIDTH - disk.radius;
        disk.vx = -disk.vx;
    }

    if (disk.y - disk.radius < 0) {
        disk.y = disk.radius;
        disk.vy = -disk.vy;
    } else if (disk.y + disk.radius > CHART_TOP) {
        disk.y = CHART_TOP - disk.radius;
        disk.vy = -disk.vy;
    }
}

void update_plot(const std::vector<Disk> &disks) {
    std::vector<int> counts(MAX_COINS_PER_DISK + 1, 0);
    for (auto &d : disks) {
        counts[d.coin_count]++;
    }

    for (int i = 0; i <= MAX_COINS_PER_DISK; i++) {
        cumulative_counts[i] += counts[i];
    }

    for (int i = 0; i <= MAX_COINS_PER_DISK; i++) {
        xdata[i].push_back(static_cast<float>(collision_count));
        float avgNum = 0.f;
        if (collision_count > 0) {
            avgNum = static_cast<float>(cumulative_counts[i]) / collision_count;
        }
        ydata[i].push_back(avgNum);
        g_coinFraction[i] = avgNum;
    }
}

void draw_line_graph(sf::RenderWindow &window) {
    if (collision_count < 1) return;

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

    auto scaleY = [&](float val) {
        if (val > DISK_COUNT) val = (float)DISK_COUNT;
        float proportion = val / (float)DISK_COUNT;
        return chartY + chartHt - (proportion * chartHt);
    };

    // Tick marks
    for (int val = 0; val <= DISK_COUNT; val += 2) {
        float yPos = scaleY(static_cast<float>(val));

        sf::RectangleShape tick(sf::Vector2f(5.f, 1.f));
        tick.setFillColor(sf::Color::White);
        tick.setPosition(sf::Vector2f(chartX - 2.f, yPos));
        window.draw(tick);

        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << val;
        sf::Text label(g_font, ss.str(), 12);

        auto lb = label.getLocalBounds();
        label.setOrigin(sf::Vector2f(lb.size.x, lb.size.y * 0.5f));
        label.setPosition(sf::Vector2f(chartX + 8.f, yPos));
        label.setFillColor(sf::Color::White);
        window.draw(label);
    }

    auto scaleX = [&](float xVal) {
        if (collision_count == 0) return chartX;
        return chartX + (xVal / (float)collision_count) * chartWidth;
    };

    // Generate colors for up to MAX_COINS_PER_DISK + 1 lines
    std::vector<sf::Color> colors;
    for (int i = 0; i <= MAX_COINS_PER_DISK; i++) {
        float hue = (360.f * i) / (MAX_COINS_PER_DISK + 1);
        // Simple HSV to RGB conversion for variety of colors
        float x = 1.f - std::abs(std::fmod(hue / 60.f, 2.f) - 1.f);
        float r = 0.f, g = 0.f, b = 0.f;
        if (hue < 60) { r = 1.f; g = x; }
        else if (hue < 120) { r = x; g = 1.f; }
        else if (hue < 180) { g = 1.f; b = x; }
        else if (hue < 240) { g = x; b = 1.f; }
        else if (hue < 300) { r = x; b = 1.f; }
        else { r = 1.f; b = x; }
        colors.emplace_back(sf::Color(
            static_cast<uint8_t>(r * 255),
            static_cast<uint8_t>(g * 255),
            static_cast<uint8_t>(b * 255)
        ));
    }

    for (int i = 0; i <= MAX_COINS_PER_DISK; i++) {
        sf::VertexArray lineStrip(sf::PrimitiveType::LineStrip);
        for (size_t k = 0; k < xdata[i].size(); k++) {
            float px = scaleX(xdata[i][k]);
            float py = scaleY(ydata[i][k]);

            sf::Vertex v;
            v.position = sf::Vector2f(px, py);
            v.color = colors[i];
            lineStrip.append(v);
        }
        window.draw(lineStrip);
    }
}

void draw_stats_window(sf::RenderWindow &stats) {
    stats.clear(sf::Color(50, 50, 50));

    sf::Text title(g_font, "Coin Fractions", 18);
    title.setFillColor(sf::Color::White);
    title.setPosition(sf::Vector2f(10.f, 10.f));
    stats.draw(title);

    sf::Text collisionsText(g_font, "Collisions: " + std::to_string(collision_count), 16);
    collisionsText.setFillColor(sf::Color::White);
    collisionsText.setPosition(sf::Vector2f(10.f, 35.f));
    stats.draw(collisionsText);

    float yOffset = 60.f;
    int columns = 2;
    float columnWidth = 140.f;

    for (int c = 0; c <= MAX_COINS_PER_DISK; c++) {
        std::stringstream ss;
        ss << c << " coins = "
           << std::fixed << std::setprecision(2) << g_coinFraction[c];

        sf::Text line(g_font, ss.str(), 14);
        line.setFillColor(sf::Color::White);
        
        // Calculate position in 2 columns
        int column = c % columns;
        int row = c / columns;
        float x = 10.f + column * columnWidth;
        float y = yOffset + row * 25.f;
        
        line.setPosition(sf::Vector2f(x, y));
        stats.draw(line);
    }

    stats.display();
}

int main() {
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<float> velDist(-200.f, 200.f);

    if (!g_font.openFromFile("/System/Library/Fonts/SFNSMono.ttf")) {
        std::cerr << "Failed to open font. Check path!\n";
        return 1;
    }

    sf::RenderWindow mainWindow(sf::VideoMode({(unsigned)WIDTH, (unsigned)HEIGHT}),
                                "SFML3 Disks + Chart");
    mainWindow.setFramerateLimit(FPS);

    sf::RenderWindow statsWindow(sf::VideoMode({300, 600}), "Coin Stats");
    statsWindow.setFramerateLimit(FPS);

    // Create disks with non-overlapping positions
    std::vector<Disk> disks;
    disks.reserve(DISK_COUNT);

    // Initial coin distribution (first disk gets MAX_COINS_PER_DISK, rest get 0)
    for (int i = 0; i < DISK_COUNT; i++) {
        // Try to find a valid position for the new disk
        auto posOpt = find_valid_position(disks, rng, DISK_RADIUS);
        
        if (!posOpt) {
            std::cerr << "Failed to place disk " << i << ". Space too crowded!\n";
            return 1;
        }

        auto [x, y] = *posOpt;
        float vx = velDist(rng);
        float vy = velDist(rng);
        
        // First disk gets all coins, rest get 0
        int initial_coins = (i == 0) ? MAX_COINS_PER_DISK : 0;
        
        disks.push_back(Disk{x, y, vx, vy, DISK_RADIUS, initial_coins});
    }

    bool mainRunning = true;
    bool statsRunning = true;

    float time_since_plot = 0.f;
    sf::Clock clock;

    // Main loop that handles both windows
    while (mainRunning || statsRunning) {
        float dt = clock.restart().asSeconds();

        // Poll events from mainWindow
        if (mainRunning && mainWindow.isOpen()) {
            while (auto eOpt = mainWindow.pollEvent()) {
                sf::Event e = *eOpt;

                if (e.is<sf::Event::Closed>()) {
                    mainWindow.close();
                    mainRunning = false;
                    break;
                }

                if (const auto* keyPressed = e.getIf<sf::Event::KeyPressed>()) {
                    if (keyPressed->scancode == sf::Keyboard::Scan::Up) {
                        g_speedFactor *= 1.2f;
                    } else if (keyPressed->scancode == sf::Keyboard::Scan::Down) {
                        g_speedFactor /= 1.2f;
                        if (g_speedFactor < 0.001f) {
                            g_speedFactor = 0.001f;
                        }
                    }
                }
            }
        }

        // Poll events from statsWindow
        if (statsRunning && statsWindow.isOpen()) {
            while (auto eOpt = statsWindow.pollEvent()) {
                sf::Event e = *eOpt;
                if (e.is<sf::Event::Closed>()) {
                    statsWindow.close();
                    statsRunning = false;
                    break;
                }
            }
        }

        // Update simulation if main window is running
        if (mainRunning && mainWindow.isOpen()) {
            // Update positions
            for (auto &d : disks) {
                update_position(d, dt);
            }

            // Check for collisions
            int collisions_this_frame = 0;
            for (int i = 0; i < DISK_COUNT; i++) {
                for (int j = i+1; j < DISK_COUNT; j++) {
                    if (handle_disk_collision(disks[i], disks[j], rng)) {
                        collisions_this_frame++;
                    }
                }
            }
            collision_count += collisions_this_frame;

            // Update plot data if there were collisions
            time_since_plot += dt;
            if (time_since_plot >= 0.1f && collision_count > 0) {
                update_plot(disks);
                time_since_plot = 0.f;
            }

            // Render main window
            mainWindow.clear(sf::Color::Black);

            // Draw disks
            for (auto &d : disks) {
                // Draw circle
                sf::CircleShape circle(d.radius);
                circle.setFillColor(sf::Color(0,128,255));
                circle.setPosition(sf::Vector2f(d.x - d.radius, d.y - d.radius));
                mainWindow.draw(circle);

                // Draw coin count
                sf::Text text(g_font, std::to_string(d.coin_count), 20); // Reduced font size
                text.setFillColor(sf::Color::White);
                auto bounds = text.getLocalBounds();
                text.setOrigin(sf::Vector2f(bounds.size.x*0.5f, bounds.size.y*0.5f));
                text.setPosition(sf::Vector2f(d.x, d.y));
                mainWindow.draw(text);
            }

            // Draw line graph
            draw_line_graph(mainWindow);
            mainWindow.display();
        }

        // Update stats window if it's running
        if (statsRunning && statsWindow.isOpen()) {
            draw_stats_window(statsWindow);
        }

        // Exit if both windows are closed
        if (!mainRunning && !statsRunning) {
            break;
        }
    }

    return 0;
}