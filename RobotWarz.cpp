#include <iostream>
#include "Arena.h"

int main(int argc, char* argv[]) {
    std::cout << "Starting Robot Warz..." << std::endl;
    if (argc < 2) {
        std::cerr << "Please provide a config file." << std::endl;
        return 1;
    }

    Arena arena;

    arena.load_config(argv[1]);
    arena.spawn_obstacles(arena.m_mounds, arena.m_pits, arena.m_flamethrowers);
    arena.load_robots();
    
    arena.run_game();

    return 0;
}