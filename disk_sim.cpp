#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <matplot/matplot.h>

#include <cmath>
#include <vector>
#include <random>
#include <string>
#include <iostream>
#include <thread>       // for sleeping (optional)
#include <chrono>

static const int WIDTH = 800;
static const int HEIGHT = 600;
static const int FPS = 60;
static const int DISK_RADIUS = 40;
static const int DISK_COUNT = 6;
static const int MAX_COINS_PER_DISK = 8;

// Global collision count
static int collision_count = 0;
// Global cumulative counts for coin states 0..8
static std::vector<int> cumulative_counts(9, 0);

struct Disk {
    float x;
    float y;
    float vx;
    float vy;
    int radius;
    int coin_count;
};

float distance(Disk &a, Disk &b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return std::sqrt(dx*dx + dy*dy);
}

// Check collision, do coin exchange, return true if collided
bool handle_disk_collision(Disk &d1, Disk &d2, std::mt19937 &rng) {
    float dist = distance(d1, d2);
    if (dist < d1.radius + d2.radius) {
        float nx = (d2.x - d1.x) / dist;
        float ny = (d2.y - d1.y) / dist;
        float v1n = d1.vx * nx + d1.vy * ny;
        float v2n = d2.vx * nx + d2.vy * ny;

        // swap normal velocity components
        d1.vx += (v2n - v1n) * nx;
        d1.vy += (v2n - v1n) * ny;
        d2.vx += (v1n - v2n) * nx;
        d2.vy += (v1n - v2n) * ny;

        // coin exchange
        std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
        int total_coins_d1 = d1.coin_count;
        int total_coins_d2 = d2.coin_count;

        int coins_moving_to_d2 = 0;
        for (int i = 0; i < total_coins_d1; i++) {
            if (dist01(rng) < 0.5f) {
                coins_moving_to_d2++;
            }
        }
        d1.coin_count -= coins_moving_to_d2;
        d2.coin_count += coins_moving_to_d2;

        int coins_moving_to_d1 = 0;
        for (int i = 0; i < total_coins_d2; i++) {
            if (dist01(rng) < 0.5f) {
                coins_moving_to_d1++;
            }
        }
        d2.coin_count -= coins_moving_to_d1;
        d1.coin_count += coins_moving_to_d1;

        if (d1.coin_count > MAX_COINS_PER_DISK) d1.coin_count = MAX_COINS_PER_DISK;
        if (d2.coin_count > MAX_COINS_PER_DISK) d2.coin_count = MAX_COINS_PER_DISK;

        return true;
    }
    return false;
}

void update_position(Disk &disk, float dt) {
    disk.x += disk.vx * dt;
    disk.y += disk.vy * dt;

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
    } else if (disk.y + disk.radius > HEIGHT) {
        disk.y = HEIGHT - disk.radius;
        disk.vy = -disk.vy;
    }
}

// Matplot++ data
static matplot::figure_handle fig;
static matplot::axes_handle ax;

static std::vector<std::vector<double>> xdata(9), ydata(9);
// We'll store line handles for each of the 9 lines
static std::vector<matplot::line_handle> plot_lines(9);

void setup_matplot_lines() {
    // Create figure in its own window
    // Depending on your Matplot++ version, (true) might or might not open new window
    fig = matplot::figure(true);
    ax = fig->current_axes();
    ax->title("Running Average of Coin Counts");

    std::vector<std::string> colors = {
        "b", "r", "g", "c", "m", "y", "k", "#7f7f7f", "#ff7f0e"
    };
    std::vector<std::string> labels = {
        "0 coins","1 coin","2 coins","3 coins","4 coins",
        "5 coins","6 coins","7 coins","8 coins"
    };
    matplot::hold(ax, true);

    for (int i = 0; i < 9; i++) {
        // 'plot(...)' might return a single handle or a vector<line_handle>
        // For older versions that return a single handle, we can do:
        auto h = matplot::plot(ax, xdata[i], ydata[i]);
        // 'plot(...)' can return a vector if you pass multiple series, but here it's just one

        h->color(colors[i]);
        h->display_name(labels[i]);
        plot_lines[i] = h;
    }

    matplot::xlabel(ax, "Collision Count");
    matplot::ylabel(ax, "Running Average Fraction of Disks");
    matplot::legend(ax);
    matplot::ylim(ax, {0.0, 1.0});
    matplot::xlim(ax, {0.0, 10.0});

    // show once
    matplot::show();
    matplot::hold(ax, false);
}

void update_plot(const std::vector<Disk> &disks) {
    // count how many disks have each coin count
    std::vector<int> counts(9, 0);
    for (auto &d : disks) {
        counts[d.coin_count]++;
    }

    // update global cumulative counts
    for (int i = 0; i < 9; i++) {
        cumulative_counts[i] += counts[i];
    }

    // now compute fraction
    for (int i = 0; i < 9; i++) {
        xdata[i].push_back((double)collision_count);
        double fraction = (double)cumulative_counts[i] / (DISK_COUNT * collision_count);
        ydata[i].push_back(fraction);

        // update line
        plot_lines[i]->x_data(xdata[i]);
        plot_lines[i]->y_data(ydata[i]);
    }

    // adjust x-limits if collisions > 10
    if (collision_count > 10) {
        matplot::xlim(ax, {0.0, (double)collision_count});
    }

    // redraw figure
    // Some older versions only have fig->draw() or show()
    // We'll do fig->draw() to force re-render
    fig->draw();

    // to mimic a short pause for updates (like pause(0.001)):
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

int main(int argc, char* argv[]) {
    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_real_distribution<float> velDist(-200.f, 200.f);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << "\n";
        return 1;
    }
    if (TTF_Init() == -1) {
        std::cerr << "TTF_Init Error: " << TTF_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Bouncing Disks (C++ Matplot)",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << "\n";
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    TTF_Font* font = TTF_OpenFont("/System/Library/Fonts/Helvetica.ttc", 24);
    if (!font) {
        std::cerr << "TTF_OpenFont Error: " << TTF_GetError() << "\n";
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Setup the matplot figure/lines
    setup_matplot_lines();

    // create disks
    std::vector<Disk> disks(DISK_COUNT);
    std::vector<int> distribution = {8, 0, 0, 0, 0, 0};
    for (int i = 0; i < DISK_COUNT; i++) {
        float x = (float)(DISK_RADIUS + rand() % (WIDTH - 2*DISK_RADIUS));
        float y = (float)(DISK_RADIUS + rand() % (HEIGHT - 2*DISK_RADIUS));
        float vx = velDist(rng);
        float vy = velDist(rng);
        disks[i] = Disk{x,y,vx,vy,DISK_RADIUS,distribution[i]};
    }

    bool running = true;
    Uint32 lastTicks = SDL_GetTicks();

    while (running) {
        Uint32 currentTicks = SDL_GetTicks();
        float dt = (currentTicks - lastTicks) / 1000.0f;
        lastTicks = currentTicks;

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            }
        }

        // update positions
        for (auto &d : disks) {
            update_position(d, dt);
        }
        // collisions
        for (int i = 0; i < DISK_COUNT; i++) {
            for (int j = i+1; j < DISK_COUNT; j++) {
                bool collided = handle_disk_collision(disks[i], disks[j], rng);
                if (collided) {
                    collision_count++;
                    update_plot(disks);
                }
            }
        }

        // render SDL
        SDL_SetRenderDrawColor(renderer, 0,0,0,255);
        SDL_RenderClear(renderer);

        for (auto &d : disks) {
            // draw circle
            SDL_SetRenderDrawColor(renderer, 0,128,255,255);
            for (int w = -d.radius; w <= d.radius; w++) {
                for (int h = -d.radius; h <= d.radius; h++) {
                    if (w*w + h*h <= d.radius*d.radius) {
                        int px = (int)d.x + w;
                        int py = (int)d.y + h;
                        SDL_RenderDrawPoint(renderer, px, py);
                    }
                }
            }
            // coin count
            SDL_Color textColor = {255,255,255,255};
            std::string text = std::to_string(d.coin_count);
            SDL_Surface* surf = TTF_RenderText_Solid(font, text.c_str(), textColor);
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                if (tex) {
                    int tw=0, th=0;
                    SDL_QueryTexture(tex, nullptr, nullptr, &tw,&th);
                    SDL_Rect dst{(int)d.x - tw/2, (int)d.y - th/2, tw, th};
                    SDL_RenderCopy(renderer, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
                }
                SDL_FreeSurface(surf);
            }
        }

        SDL_RenderPresent(renderer);

        // limit ~60 fps
        Uint32 frameTime = SDL_GetTicks() - currentTicks;
        if (frameTime < (1000 / FPS)) {
            SDL_Delay((1000 / FPS) - frameTime);
        }
    }

    // cleanup
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    // If your version has a "close()" that takes a figure_handle, you can do:
    // matplot::close(fig);

    return 0;
}
