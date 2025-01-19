/*
 * disk_sim.cpp (SFML 3)
 *
 * Features:
 *   - Bouncing Disks with Overlap Fix
 *   - Real-time line chart (0..0.5 scale) with visible tick labels
 *   - Second Window showing y-values of each line
 *   - Up/Down arrow keys to change disk speed
 */

#include <SFML/Graphics.hpp>
#include <optional>
#include <random>
#include <cmath>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>  // for std::setprecision

// ---------------------
// GLOBAL CONSTANTS
// ---------------------
static const int WIDTH  = 800;
static const int HEIGHT = 600;
static const int FPS    = 60;

static const int DISK_RADIUS         = 40;
static const int DISK_COUNT          = 6;
static const int MAX_COINS_PER_DISK  = 8;

// We'll reserve ~200px at bottom for the chart
static const float CHART_TOP    = 400.f;
static const float CHART_HEIGHT = 200.f;

// Global speed factor changed by Up/Down arrow
static float g_speedFactor = 5.0f; // 1.0 = normal speed

// ---------------------
// GLOBALS FOR CHART
// ---------------------
static int collision_count = 0;  // track total collisions

// Each coin count (0..8): store x (collision_count) and fraction
static std::vector<float> xdata[9];
static std::vector<float> ydata[9];
static std::vector<int>   cumulative_counts(9, 0);

// We'll also store the latest fraction for each coin count (0..8),
// so we can display them in the second window (3 decimal places).
static float g_coinFraction[9] = {0.f};

// We'll load one global font for everything
static sf::Font g_font;

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

// -------------------------------------------------------------
// handle_disk_collision: bounce + coin exchange + overlap fix
// -------------------------------------------------------------
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

        // Coin exchange (random)
        std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
        int total_coins_d1 = d1.coin_count;
        int total_coins_d2 = d2.coin_count;

        // Special case: Handle 0-coin disks
        if (d1.coin_count == 0 && d2.coin_count > 0) {
            if (dist01(rng) < 0.5f) {
                d1.coin_count++;
                d2.coin_count--;
            }
        }
        if (d2.coin_count == 0 && d1.coin_count > 0) {
            if (dist01(rng) < 0.5f) {
                d2.coin_count++;
                d1.coin_count--;
            }
        }

        // Standard coin exchange (50% chance for each coin)
        int coins_to_d2 = 0;
        for (int i = 0; i < total_coins_d1; i++) {
            if (dist01(rng) < 0.5f) {
                coins_to_d2++;
            }
        }
        d1.coin_count -= coins_to_d2;
        d2.coin_count += coins_to_d2;

        int coins_to_d1 = 0;
        for (int i = 0; i < total_coins_d2; i++) {
            if (dist01(rng) < 0.5f) {
                coins_to_d1++;
            }
        }
        d2.coin_count -= coins_to_d1;
        d1.coin_count += coins_to_d1;

        // Clamp
        if (d1.coin_count > MAX_COINS_PER_DISK) d1.coin_count = MAX_COINS_PER_DISK;
        if (d2.coin_count > MAX_COINS_PER_DISK) d2.coin_count = MAX_COINS_PER_DISK;


        // Overlap fix
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

// ------------------------------------
// update_position: uses g_speedFactor
// ------------------------------------
void update_position(Disk &disk, float dt) {
    // Multiply velocities by dt * g_speedFactor
    disk.x += disk.vx * dt * g_speedFactor;
    disk.y += disk.vy * dt * g_speedFactor;

    // bounce off left/right
    if (disk.x - disk.radius < 0) {
        disk.x = disk.radius;
        disk.vx = -disk.vx;
    } else if (disk.x + disk.radius > WIDTH) {
        disk.x = WIDTH - disk.radius;
        disk.vx = -disk.vx;
    }

    // bounce off top/bottom (we treat CHART_TOP as bottom)
    if (disk.y - disk.radius < 0) {
        disk.y = disk.radius;
        disk.vy = -disk.vy;
    } else if (disk.y + disk.radius > CHART_TOP) {
        disk.y = CHART_TOP - disk.radius;
        disk.vy = -disk.vy;
    }
}

// -------------------------------------------------------------
// update_plot: record fraction of disks with 0..8 coins
// also store them in g_coinFraction
// -------------------------------------------------------------
void update_plot(const std::vector<Disk> &disks) {
    // how many disks have each coin count
    std::vector<int> counts(9, 0);
    for (auto &d : disks) {
        counts[d.coin_count]++;
    }

    // update global cumulative_counts
    for (int i = 0; i < 9; i++) {
        cumulative_counts[i] += counts[i];
    }

    // push back fraction
    for (int i = 0; i < 9; i++) {
        xdata[i].push_back(static_cast<float>(collision_count));

        float avgNum = 0.f;
        if (collision_count > 0) {
            // average number of disks = (total count of i-coins) / number_of_collisions
            avgNum = static_cast<float>(cumulative_counts[i]) / collision_count;
        }
        ydata[i].push_back(avgNum);
        g_coinFraction[i] = avgNum;
    }
}

// ---------------------------------------------
// draw_line_graph: bottom 200px, range 0..0.5
// with tick marks 0.0..0.5
// ---------------------------------------------
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

    // Change to range 0..6:
    auto scaleY = [&](float val) {
        if (val > 6.f) val = 6.f;
        float proportion = val / 6.f; // 0..1
        return chartY + chartHt - (proportion * chartHt);
    };


    // Tick marks at integer steps 0..6:
    for (int val = 0; val <= 6; val++) {
        float yPos = scaleY(static_cast<float>(val));

        // short tick line
        sf::RectangleShape tick(sf::Vector2f(5.f, 1.f));
        tick.setFillColor(sf::Color::White);
        tick.setPosition(sf::Vector2f(chartX - 2.f, yPos));
        window.draw(tick);

        // label: use fixed decimal (1 digit or so)
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << val;
        sf::Text label(g_font, ss.str(), 12);

        auto lb = label.getLocalBounds();
        label.setOrigin(sf::Vector2f(lb.size.x, lb.size.y * 0.5f));
        label.setPosition(sf::Vector2f(chartX + 8.f, yPos));
        label.setFillColor(sf::Color::White);
        window.draw(label);
    }

    // scaleX (0..collision_count => 0..chartWidth)
    auto scaleX = [&](float xVal) {
        if (collision_count == 0) return chartX;
        return chartX + (xVal / (float)collision_count) * chartWidth;
    };

    // 9 lines (0..8 coin counts)
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

// ----------------------------------------------------
// draw_stats_window: show the 9 fractions with 3 decimals
// ----------------------------------------------------
void draw_stats_window(sf::RenderWindow &stats) {
    // Just clear to dark grey
    stats.clear(sf::Color(50, 50, 50));

    // Title
    {
        sf::Text title(g_font, "Coin Fractions", 18);
        title.setFillColor(sf::Color::White);
        auto tb = title.getLocalBounds();
        title.setPosition(sf::Vector2f(10.f, 10.f));
        stats.draw(title);
    }

    // Now show total collisions:
    {
        sf::Text collisionsText(g_font, "Collisions: " + std::to_string(collision_count), 16);
        collisionsText.setFillColor(sf::Color::White);
        collisionsText.setPosition(sf::Vector2f(10.f, 35.f));
        stats.draw(collisionsText);
    }
    // For each coin count 0..8, show fraction w/ 3 decimals
    float yOffset = 60.f;

    for (int c = 0; c < 9; c++) {
        std::stringstream ss;
        ss << c << " coins = "
           << std::fixed << std::setprecision(2) << g_coinFraction[c];

        sf::Text line(g_font, ss.str(), 14);
        line.setFillColor(sf::Color::White);
        line.setPosition(sf::Vector2f(10.f, yOffset));
        yOffset += 25.f;

        stats.draw(line);
    }

    stats.display();
}

int main() {
    // Setup random
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<float> velDist(-200.f, 200.f);

    // Load our global font
    if (!g_font.openFromFile("/System/Library/Fonts/SFNSMono.ttf")) {
        std::cerr << "Failed to open font. Check path!\n";
    }

    // Main simulation window
    sf::RenderWindow mainWindow(sf::VideoMode({(unsigned)WIDTH, (unsigned)HEIGHT}),
                                "SFML3 Disks + Chart");
    mainWindow.setFramerateLimit(FPS);

    // Second stats window
    sf::RenderWindow statsWindow(sf::VideoMode({300, 300}), "Coin Stats");
    statsWindow.setFramerateLimit(FPS);

    // Create disks
    std::vector<Disk> disks(DISK_COUNT);
    std::vector<int> distribution = {8, 0, 0, 0, 0, 0};
    for (int i = 0; i < DISK_COUNT; i++) {
        float x  = (float)(DISK_RADIUS + rand() % (int(CHART_TOP) - 2*DISK_RADIUS));
        float y  = (float)(DISK_RADIUS + rand() % (int(CHART_TOP) - 2*DISK_RADIUS));
        float vx = velDist(rng);
        float vy = velDist(rng);
        // no initial speedFactor here, we apply g_speedFactor only in update_position
        disks[i] = Disk{x, y, vx, vy, DISK_RADIUS, distribution[i]};
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
    // Inside main loop, polling mainWindow events:

            while (auto eOpt = mainWindow.pollEvent()) {
                sf::Event e = *eOpt;

                // Handle window close
                if (e.is<sf::Event::Closed>()) {
                    mainWindow.close();
                    mainRunning = false;
                    break;
                }

                // Handle keypresses
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

                // If user closes stats window
                if (e.is<sf::Event::Closed>()) {
                    statsWindow.close();
                    statsRunning = false;
                    break;
                }
            }
        }

        // If main window is still running, update the simulation
        if (mainRunning && mainWindow.isOpen()) {
            // Update positions
            for (auto &d : disks) {
                update_position(d, dt);
            }

            // Collisions
            int collisions_this_frame = 0;
            for (int i = 0; i < DISK_COUNT; i++) {
                for (int j = i+1; j < DISK_COUNT; j++) {
                    if (handle_disk_collision(disks[i], disks[j], rng)) {
                        collisions_this_frame++;
                    }
                }
            }
            collision_count += collisions_this_frame;

            // Chart update every 0.1s if collisions occurred
            time_since_plot += dt;
            if (time_since_plot >= 0.1f && collision_count > 0) {
                update_plot(disks);
                time_since_plot = 0.f;
            }

            // Render main window
            mainWindow.clear(sf::Color::Black);

            // Draw disks
            for (auto &d : disks) {
                // Circle
                sf::CircleShape circle(d.radius);
                circle.setFillColor(sf::Color(0,128,255));
                circle.setPosition(sf::Vector2f(d.x - d.radius, d.y - d.radius));
                mainWindow.draw(circle);

                // Coin count
                sf::Text text(g_font, std::to_string(d.coin_count), 24);
                text.setFillColor(sf::Color::White);
                auto bounds = text.getLocalBounds();
                text.setOrigin(sf::Vector2f(bounds.size.x*0.5f, bounds.size.y*0.5f));
                text.setPosition(sf::Vector2f(d.x, d.y));
                mainWindow.draw(text);
            }

            // Draw chart
            draw_line_graph(mainWindow);

            mainWindow.display();
        }

        // If stats window is still running, draw the stats
        if (statsRunning && statsWindow.isOpen()) {
            draw_stats_window(statsWindow);
        }

        // If both windows are closed, we exit the loop
        if (!mainRunning && !statsRunning) {
            break;
        }
    }

    return 0;
}
