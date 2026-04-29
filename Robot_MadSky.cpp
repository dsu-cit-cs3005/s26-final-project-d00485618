#include "RobotBase.h"
#include <vector>
#include <iostream>
#include <algorithm>

class Robot_MadSky : public RobotBase {
    private:
        bool m_moving_right = true;
        bool m_at_top = false; 
        bool m_scan_down = false; 
        int to_shoot_row = -1;
        int to_shoot_col = -1;
        int m_last_radar_dir = 5; 

        std::vector<RadarObj> known_obstacles;

        void clear_target() { to_shoot_row = -1; to_shoot_col = -1; }

        void add_obstacle(const RadarObj& obj) {
            if ((obj.m_type == 'M' || obj.m_type == 'P' || obj.m_type == 'F' || obj.m_type == 'X')) {
                for(auto& obs : known_obstacles) {
                    if(obs.m_row == obj.m_row && obs.m_col == obj.m_col) return;
                }
                known_obstacles.push_back(obj);
            }
        }

        bool is_blocked(int r, int c) {
            for(auto& obs : known_obstacles) {
                if(obs.m_row == r && obs.m_col == c) return true;
            }
            return false;
        }

    public:
        Robot_MadSky() : RobotBase(1, 6, railgun) {}

        virtual void get_radar_direction(int& radar_direction) override {
            int r, c;
            get_current_location(r, c);

            if (r == 0) m_at_top = true;

            if (!m_at_top) {
                radar_direction = 1; 
            } else if (to_shoot_row != -1) {
                radar_direction = m_last_radar_dir;
            } else {
                if (m_scan_down) {
                    radar_direction = 5; 
                } else {
                    radar_direction = m_moving_right ? 3 : 7; 
                }
                m_last_radar_dir = radar_direction;
                m_scan_down = !m_scan_down;
            }
        }

        virtual void process_radar_results(const std::vector<RadarObj>& radar_results) override {
            bool found_robot = false;
            int potential_row = -1;
            int potential_col = -1;

            for (const auto& obj : radar_results) {
                add_obstacle(obj);
                if (obj.m_type != '.' && obj.m_type != 'M' && obj.m_type != 'P' && obj.m_type != 'F' && obj.m_type != 'X') {
                    potential_row = obj.m_row;
                    potential_col = obj.m_col;
                    found_robot = true;
                    break;
                }
            }

            if (found_robot) {
                to_shoot_row = potential_row;
                to_shoot_col = potential_col;
            } else {
                clear_target();
            }
        }

        virtual bool get_shot_location(int& shot_row, int& shot_col) override {
            if (to_shoot_row != -1 && to_shoot_col != -1) {
                shot_row = to_shoot_row;
                shot_col = to_shoot_col;
                return true;
            }
            return false;
        }

        void get_move_direction(int& move_direction, int& move_distance) override {
            int r, c;
            get_current_location(r, c);
            if (r == 0) m_at_top = true;

            if (to_shoot_row != -1) {
                move_distance = 0; 
                return;
            }

            if (!m_at_top) {
                move_direction = 1;
                move_distance = 1;
                return;
            }

            int next_col = m_moving_right ? c + 1 : c - 1;
            if (next_col < 0 || next_col >= m_board_col_max) {
                m_moving_right = !m_moving_right;
                move_direction = m_moving_right ? 3 : 7;
                move_distance = 1;
                return;
            }

            if (is_blocked(r, next_col)) {
                if (r + 1 < m_board_row_max) {
                    move_direction = 5; 
                } else {
                    m_moving_right = !m_moving_right;
                    move_direction = m_moving_right ? 3 : 7;
                }
                move_distance = 1;
            } else {
                move_direction = m_moving_right ? 3 : 7;
                move_distance = 1;
            }
        }
};

extern "C" RobotBase* create_robot() { return new Robot_MadSky(); }
extern "C" const char* robot_summary() { return "Moves to top and moves from wall to wall ; stops and fires until target is destroyed or moves; goes down once if obstacle in path."; }