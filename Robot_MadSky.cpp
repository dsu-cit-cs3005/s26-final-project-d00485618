#include "RobotBase.h"
#include <vector>
#include <iostream>
#include <algorithm>

class Robot_MadSky : public RobotBase {
    private:
        bool m_moving_right = true;
        int to_shoot_row = -1;
        int to_shoot_col = -1;

        std::vector<RadarObj> known_obstacles;

        bool is_obstacle(int row, int col) const {
            return std::any_of(known_obstacles.begin(), known_obstacles.end(), [&](const RadarObj& obj) {
                return obj.m_row == row && obj.m_col == col;
            });
        }

        void clear_target() {
            to_shoot_row = -1;
            to_shoot_col = -1;
        }

        void add_obstacle(const RadarObj& obj) {
            if ((obj.m_type == 'M' || obj.m_type == 'P' || obj.m_type == 'F') && !is_obstacle(obj.m_row, obj.m_col)) {
                known_obstacles.push_back(obj);
            }
        }
    
    public:
        Robot_MadSky() : RobotBase(3, 4, railgun) {}

        virtual void get_radar_direction(int& radar_direction) override {
            int current_row, current_col;
            get_current_location(current_row, current_col);

            radar_direction = (current_col > 0) ? 7 : 3; // Left or Right
        }

        virtual void process_radar_results(const std::vector<RadarObj>& radar_results) override {
            clear_target();

            for (const auto& obj : radar_results) {
                // Add static obstacles to the obstacle list
                add_obstacle(obj);

                // Identify the first enemy found as the target
                if (obj.m_type == 'R' && to_shoot_row == -1 && to_shoot_col == -1) {
                    to_shoot_row = obj.m_row;
                    to_shoot_col = obj.m_col;
                }
            }
        }

        virtual bool get_shot_location(int& shot_row, int& shot_col) override 
    {
        if (to_shoot_row != -1 && to_shoot_col != -1) 
        {
            shot_row = to_shoot_row;
            shot_col = to_shoot_col;
            clear_target(); // Clear target after shooting
            return true;
        }
        return false;
    }

    // Determines the next movement direction
    void get_move_direction(int& move_direction, int& move_distance) override {
        int current_row, current_col;
        get_current_location(current_row, current_col);
        int move = get_move_speed(); // Max movement range for this robot

        // Step 1: Move up until row == 0
        if (current_row > 0) {
            move_direction = 1; // Up
            move_distance = std::min(move, current_row); // Clamp to avoid going out of bounds
            return;
        }

        // Step 2: Horizontal movement once row == 0
        if (m_moving_right) {
            // Move right if not at the right edge
            if (current_col + move < m_board_col_max) {
                move_direction = 3; // Right
                move_distance = std::min(move, m_board_col_max - current_col - 1);
            } else {
                // Hit the right wall, switch to moving left
                m_moving_right = false;
                move_direction = 7; // Left
                move_distance = 1;  
            }
        } else {
            // Move left if not at the left edge
            if (current_col - move >= 0) {
                move_direction = 7; // Left
                move_distance = std::min(move, current_col);
            } else {
                // Hit the left wall, switch to moving right
                m_moving_right = true;
                move_direction = 3; // Right
                move_distance = 1;  
            }
        }
    }
};

extern "C" RobotBase* create_robot() {
    return new Robot_MadSky();
}

extern "C" const char* robot_summary() {
    return "Hugs top wall, railguns nearest target.";
}