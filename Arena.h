#ifndef ARENA_H
#define ARENA_H

#include <vector>
#include <string>
#include <stdbool.h>
#include <utility>
#include "RobotBase.h"

struct Cell {
    char type;
    RobotBase* robotPtr;
};

class Arena {
    private:
        int m_height, m_width;
        int m_max_rounds;
        float m_sleep_interval;
        bool m_is_live;

        std::vector<std::vector<Cell>> m_grid;
        std::vector<RobotBase*> m_robots;
    public:
        int m_mounds, m_pits, m_flamethrowers;
        bool load_config(const std::string& filename);
        bool check_for_winner();
        void load_robots();
        void spawn_obstacles(int mounds, int pits, int flamethrowers);
        void run_game();

        std::vector<RadarObj> perform_radar_scan(int row, int column, int direction);
        void handle_shot(RobotBase* shooter, int target_r, int target_c);
        void handle_move(RobotBase* robot, int direction, int speed);

        void display() const;
};
#endif