#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <stdbool.h>
#include <fstream>
#include <sstream>
#include <utility>
#include <iomanip>
#include "Arena.h"

bool Arena::load_config(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) return false;

    std::string line, label;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::getline(ss, label, ':');

        if (label == "Arena_Size") {
            ss >> m_height >> m_width;
            m_grid.resize(m_height, std::vector<Cell>(m_width, {'.', nullptr}));
        }
        else if (label == "Max_Rounds") {
            ss >> m_max_rounds;
        }
        else if (label == "Sleep_interval") {
            ss >> m_sleep_interval;
        }
        else if (label == "Game_State_Live") {
            ss >> m_is_live;
        }
        else if (label == "Flamethrowers") {
            ss >> m_flamethrowers;
        }
        else if (label == "Pits") {
            ss >> m_pits;
        }
        else if (label == "Mounds") {
            ss >> m_mounds;
        }
    }
    return true;
}

void Arena::load_robots() {
    std::string symbols = "@#$%^&*";
    size_t robot_count = 0;

    std::string path = "."; 
    DIR* dir = opendir(path.c_str());
    struct dirent* entry;

    if (!dir) return;

    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;

        if (filename.find("Robot_") == 0 && filename.length() >= 4 && filename.rfind(".cpp") == (filename.length() - 4)) {
            std::string shared_lib = "./" + filename.substr(0, filename.find(".cpp")) + ".so";

            std::string compile_cmd = "g++ -shared -fPIC -o " + shared_lib + " " + filename + " RobotBase.o -I. -std=c++20";
            
            if (std::system(compile_cmd.c_str()) != 0) {
                std::cerr << "Failed to compile: " << filename << std::endl;
                continue;
            }

            void* handle = dlopen(shared_lib.c_str(), RTLD_LAZY);
            if (!handle) {
                std::cerr << "dlopen error: " << dlerror() << std::endl;
                continue;
            }

            RobotFactory create_robot = (RobotFactory)dlsym(handle, "create_robot");
            if (!create_robot) {
                std::cerr << "dlsym error: " << dlerror() << std::endl;
                dlclose(handle);
                continue;
            }

            RobotBase* robot = create_robot();
            if (robot) {
                std::string cleanName = filename;
                size_t start = cleanName.find("_");
                size_t end = cleanName.find(".cpp");

                if (start != std::string::npos && end != std::string::npos) {
                    cleanName = cleanName.substr(start + 1, end - start - 1);
                    robot->m_name = cleanName;
                }

                bool placed = false;
                while (!placed) {
                    int start_r = std::rand() % m_height;
                    int start_c = std::rand() % m_width;
                
                    if (m_grid[start_r][start_c].type == '.') {
                        robot->move_to(start_r, start_c);

                        m_grid[start_r][start_c].type = 'R';
                        m_grid[start_r][start_c].robotPtr = robot;

                        placed = true;
                    }
                }
                if (robot_count < symbols.length()) {
                    robot->m_character = symbols[robot_count % symbols.length()];
                } else {
                    robot->m_character = '?';
                }
                robot->set_boundaries(m_height, m_width);
                m_robots.push_back(robot);

                ++robot_count;
            }
        }
    }
    closedir(dir);
}

void Arena::spawn_obstacles(int mounds, int pits, int flamethrowers) {
    if (mounds + pits + flamethrowers < (m_height * m_width)) {
        for (int i = 0; i < mounds; ++i) {
            bool foundSpot = false;
            int attempts = 0;

            while (!foundSpot && attempts < 500) {
                int randRow = std::rand() % m_height;
                int randColumn = std::rand() % m_width;

                if (m_grid[randRow][randColumn].type == '.') {
                    m_grid[randRow][randColumn].type = 'M';
                    foundSpot = true;
                } 
            } 
        }

        for (int i = 0; i < pits; ++i) {
            bool foundSpot = false;
            int attempts = 0;

            while (!foundSpot && attempts < 500) {
                int randRow = std::rand() % m_height;
                int randColumn = std::rand() % m_width;

                if (m_grid[randRow][randColumn].type == '.') {
                    m_grid[randRow][randColumn].type = 'P';
                    foundSpot = true;
                } 
            }
        }

        for (int i = 0; i < flamethrowers; ++i) {
            bool foundSpot = false;
            int attempts = 0;

            while (!foundSpot && attempts < 500) {
                int randRow = std::rand() % m_height;
                int randColumn = std::rand() % m_width;

                if (m_grid[randRow][randColumn].type == '.') {
                    m_grid[randRow][randColumn].type = 'F';
                    foundSpot = true;
                } 
            } 
        }
    }
    
}

void Arena::run_game() {
    int round = 1;
    bool game_over = false;
    if (m_robots.empty()) return;

    while (game_over == false && round <= m_max_rounds) {
        std::cout << "\n=========== starting round " << round << " ===========" << std::endl;
        display();

        for (RobotBase* robot : m_robots) {
            if (robot->get_health() <= 0) continue;

            int r, c;
            robot->get_current_location(r, c);

            std::cout << robot->m_name << " " << robot->m_character 
                      << " (" << r << "," << c << ") "
                      << "Health: " << robot->get_health() 
                      << " Armor: " << robot->get_armor() << std::endl;
            
            int radar_dir;
            robot->get_radar_direction(radar_dir);
            std::vector<RadarObj> scan_results = perform_radar_scan(r, c, radar_dir);
            robot->process_radar_results(scan_results);

            int shot_r, shot_c;
            bool wants_to_shoot = robot->get_shot_location(shot_r, shot_c);

            if (wants_to_shoot) {
                handle_shot(robot, shot_r, shot_c);
            } else {
                std::cout << "  not firing" << std::endl;
                int move_dir, move_dist;
                robot->get_move_direction(move_dir, move_dist);
                handle_move(robot, move_dir, move_dist);
            }
            std::cout << std::endl;

            if (m_is_live) {
                std::this_thread::sleep_for(std::chrono::milliseconds((int)(m_sleep_interval * 1000)));
            }
        }

        if (check_for_winner() == true) {
            game_over = true;
            break;
        }
        ++round;
    }
}

bool Arena::check_for_winner() {
    int robots_alive = 0;

    for (RobotBase* robot : m_robots) {
        if (robot != nullptr && robot->get_health() > 0) {
            robots_alive++;
        }
    }

    if (robots_alive <= 1) {
        std::cout << "The battle has ended!" << std::endl;
        
        for (RobotBase* r : m_robots) {
            if (r->get_health() > 0) {
                std::cout << "Winner: " << r->m_name << std::endl;
            }
        }
        return true;
    }

    return false;
}

std::vector<RadarObj> Arena::perform_radar_scan(int row, int column, int direction) {
    std::vector<RadarObj> results;
    int d_row = 0, d_col = 0;

    if (direction == 0) {
        for (int r = row - 1; r <= row + 1; ++r) {
            for (int c = column - 1; c <= column + 1; ++c) {
                if (r == row && c == column) continue;

                if (r >= 0 && r < m_height && c >= 0 && c < m_width) {
                    if (m_grid[r][c].type != '.') {
                    }
                }
            }
        }
        return results;
    } 
    else if (direction == 1) { d_row = -1; d_col = 0;  } // Up
    else if (direction == 2) { d_row = -1; d_col = 1;  } // Up-right
    else if (direction == 3) { d_row = 0;  d_col = 1;  } // Right
    else if (direction == 4) { d_row = 1;  d_col = 1;  } // Down-right
    else if (direction == 5) { d_row = 1;  d_col = 0;  } // Down
    else if (direction == 6) { d_row = 1;  d_col = -1; } // Down-left
    else if (direction == 7) { d_row = 0;  d_col = -1; } // Left
    else if (direction == 8) { d_row = -1; d_col = -1; } // Up-left
    else {
        return results;
    }

    int current_row = row + d_row;
    int current_col = column + d_col;

    while (current_row >= 0 && current_row < m_height && current_col >= 0 && current_col < m_width) {
        if (m_grid[current_row][current_col].type != '.') {
            RadarObj obj;
            obj.m_type = m_grid[current_row][current_col].type;
            obj.m_row = current_row;
            obj.m_col = current_col;
            results.push_back(obj);
        }
        current_row += d_row;
        current_col += d_col;
    }

    std::cout << "  radar scan returned ";
    if (results.empty()) {
        std::cout << "nothing";
    } else {
        for (size_t i = 0; i < results.size(); ++i) {
            if (results[i].m_type != '.' && results[i].m_type != 'M' && results[i].m_type != 'P' && results[i].m_type != 'F' && results[i].m_type != 'X') {
                std::cout << "R" << results[i].m_type;
            } else {
                std::cout << results[i].m_type;
            }
            std::cout << " at " << results[i].m_row << "," << results[i].m_col;
            if (i < results.size() - 1) std::cout << " and ";
        }
    }
    std::cout << std::endl;
    return results;
}

void Arena::handle_shot(RobotBase* shooter, int target_r, int target_c) {
    if (shooter == nullptr || shooter->get_health() <= 0) return;
    WeaponType m_weapon = shooter->get_weapon();
    int current_row, current_col;
    int shooter_row, shooter_col;
    shooter->get_current_location(current_row, current_col);
    shooter->get_current_location(shooter_row, shooter_col);

    if (m_weapon == railgun) {
        int d_row = std::abs(target_r - current_row);
        int d_col = std::abs(target_c - current_col);
        int s_row = (current_row < target_r) ? 1 : -1;
        int s_col = (current_col < target_c) ? 1 : -1;
        int error = d_row - d_col;

        while (true) {
            if (current_row < 0 || current_row >= m_height || current_col < 0 || current_col >= m_width) break;
            if (current_row != shooter_row || current_col != shooter_col) {
                if (m_grid[current_row][current_col].robotPtr != nullptr) {
                    RobotBase* hit_robot = m_grid[current_row][current_col].robotPtr;

                    int damage_roll = (std::rand() % 11) + 10;
                    damage_roll -= (int)(hit_robot->get_armor() * 0.10);
                    hit_robot->take_damage(damage_roll);
                    hit_robot->reduce_armor(1);
                    if (hit_robot->get_health() <= 0) {
                        m_grid[current_row][current_col].type = 'X';
                        m_grid[current_row][current_col].robotPtr = nullptr;
                    }

                    std::cout << "  firing railgun at " << target_r << "," << target_c 
                              << " Hits Robot " << hit_robot->m_name << " at " << current_row << "," << current_col << std::endl;
                    std::cout << "  " << hit_robot->m_name << " takes " << damage_roll << " damage" << std::endl;
                }
            }

            int e2 = 2 * error;
            if (e2 > -d_col) {
                error -= d_col;
                current_row += s_row;
            }
            if (e2 < d_row) {
                error += d_row;
                current_col += s_col;
            }
        }
    }
    else if (m_weapon == hammer) {
        if (target_r >= 0 && target_r < m_height && target_c >= 0 && target_c < m_width) {
            if (!(target_r == shooter_row && target_c == shooter_col)) {
                if (std::abs(target_r - shooter_row) <= 1 && std::abs(target_c - shooter_col) <= 1) {
                    if (m_grid[target_r][target_c].robotPtr != nullptr) {
                        RobotBase* hit_robot = m_grid[target_r][target_c].robotPtr;

                        int damage_roll = (std::rand() % 11) + 50;
                        damage_roll -= (int)(hit_robot->get_armor() * 0.10);
                        hit_robot->take_damage(damage_roll);
                        hit_robot->reduce_armor(1);
                        if (hit_robot->get_health() <= 0) {
                            m_grid[target_r][target_c].type = 'X';
                            m_grid[target_r][target_c].robotPtr = nullptr;
                        }

                        std::cout << "  swinging hammer at " << target_r << "," << target_c 
                              << " Hits Robot " << hit_robot->m_name << " at " << current_row << "," << current_col << std::endl;
                        std::cout << "  " << hit_robot->m_name << " takes " << damage_roll << " damage" << std::endl;
                    }
                }
            }
        }
    }
    else if (m_weapon == grenade) {
        if (shooter->get_grenades() > 0) {
            for (int c = target_c - 1; c <= target_c + 1; ++c) {
                for (int r = target_r - 1; r <= target_r + 1; ++r) {
                    if (r >= 0 && r < m_height && c >= 0 && c < m_width) {
                        if (m_grid[r][c].robotPtr != nullptr) {
                            RobotBase* hit_robot = m_grid[r][c].robotPtr;

                            int damage_roll = (std::rand() % 31) + 10;
                            damage_roll -= (int)(hit_robot->get_armor() * 0.10);
                            hit_robot->take_damage(damage_roll);
                            hit_robot->reduce_armor(1);
                            if (hit_robot->get_health() <= 0) {
                                m_grid[r][c].type = 'X';
                                m_grid[r][c].robotPtr = nullptr;
                            }

                            std::cout << "  launching grenade at " << target_r << "," << target_c 
                              << " Hits Robot " << hit_robot->m_name << " at " << current_row << "," << current_col << std::endl;
                            std::cout << "  " << hit_robot->m_name << " takes " << damage_roll << " damage" << std::endl;
                        }
                    }
                }
            }
            shooter->decrement_grenades();
        }
    }
    else if (m_weapon == flamethrower) {
        int d_row = 0, d_col = 0;
    
        if (target_r < shooter_row) d_row = -1;      // Up
        else if (target_r > shooter_row) d_row = 1;  // Down
        else if (target_c > shooter_col) d_col = 1;  // Right
        else if (target_c < shooter_col) d_col = -1; // Left

        for (int i = 1; i <= 4; ++i) {
            int center_r = shooter_row + (d_row * i);
            int center_c = shooter_col + (d_col * i);

            for (int j = -1; j <= 1; ++j) {
                int final_r = center_r + (d_col * j); 
                int final_c = center_c + (d_row * j);

                if (final_r >= 0 && final_r < m_height && final_c >= 0 && final_c < m_width) {
                    if (m_grid[final_r][final_c].robotPtr != nullptr) {
                        RobotBase* hit_robot = m_grid[final_r][final_c].robotPtr;

                        int damage_roll = (std::rand() % 21) + 30;
                        damage_roll -= (int)(hit_robot->get_armor() * 0.10);
                        hit_robot->take_damage(damage_roll);
                        hit_robot->reduce_armor(1);
                        if (hit_robot->get_health() <= 0) {
                            m_grid[final_r][final_c].type = 'X';
                            m_grid[final_r][final_c].robotPtr = nullptr;
                        }

                        std::cout << "  firing flamethrower at " << target_r << "," << target_c 
                              << " Hits Robot " << hit_robot->m_name << " at " << current_row << "," << current_col << std::endl;
                        std::cout << "  " << hit_robot->m_name << " takes " << damage_roll << " damage" << std::endl;
                    }
                }
            }
        }
    }
}

void Arena::handle_move(RobotBase* robot, int direction, int speed) {
    if (robot == nullptr || robot->get_health() <= 0) return;

    int m_direction = direction, m_speed = speed;
    if (m_speed > robot->get_move_speed()) m_speed = robot->get_move_speed();
    if (m_speed < 0) m_speed = 0;

    int end_r, end_c;
    int current_r, current_c;
    robot->get_current_location(current_r, current_c);

    int r_offset, c_offset;

    if (m_direction == 1) r_offset = -1, c_offset = 0;       // up
    else if (m_direction == 2) r_offset = -1, c_offset = 1;  // up right
    else if (m_direction == 3) r_offset = 0, c_offset = 1;  // right
    else if (m_direction == 4) r_offset = 1, c_offset = 1;  // down right
    else if (m_direction == 5) r_offset = 1, c_offset = 0;  // down
    else if (m_direction == 6) r_offset = 1, c_offset = -1;  // down left
    else if (m_direction == 7) r_offset = 0, c_offset = -1;  // left
    else if (m_direction == 8) r_offset = -1, c_offset = -1;  // up left
    

    end_r = current_r;
    end_c = current_c;
    for (int i = 0; i < m_speed; ++i) {
        end_r += r_offset;
        end_c += c_offset;
        if (end_r < 0 || end_r >= m_height || end_c < 0 || end_c >= m_width) {
            end_r -= r_offset;
            end_c -= c_offset; 
            m_grid[current_r][current_c].type = '.';
            m_grid[current_r][current_c].robotPtr = nullptr;
            m_grid[end_r][end_c].type = robot->m_character;
            m_grid[end_r][end_c].robotPtr = robot;
            std::cout << "  moving to (" << end_r << "," << end_c << ")" << std::endl;
            robot->move_to(end_r, end_c);
            return;
        }
        if (m_grid[end_r][end_c].type == 'P') {
            robot->disable_movement();
            m_grid[current_r][current_c].type = '.';
            m_grid[current_r][current_c].robotPtr = nullptr;
            m_grid[end_r][end_c].type = robot->m_character;
            m_grid[end_r][end_c].robotPtr = robot;
            std::cout << "  moving to (" << end_r << "," << end_c << ")" << std::endl;
            robot->move_to(end_r, end_c);
            return;
        }
        else if (m_grid[end_r][end_c].type == 'F') {
            int damage_roll = (std::rand() % 21) + 30;
            damage_roll -= (int)(robot->get_armor() * 0.10);
            robot->take_damage(damage_roll);
            robot->reduce_armor(1);
            if (robot->get_health() <= 0) {
                m_grid[current_r][current_c].type = '.';
                m_grid[current_r][current_c].robotPtr = nullptr;
                robot->move_to(end_r, end_c);
                m_grid[end_r][end_c].type = 'X';
                m_grid[end_r][end_c].robotPtr = nullptr;
                return;
            }
        }
        else if (m_grid[end_r][end_c].type != '.') {
            end_r -= r_offset;
            end_c -= c_offset; 
            m_grid[current_r][current_c].type = '.';
            m_grid[current_r][current_c].robotPtr = nullptr;
            m_grid[end_r][end_c].type = robot->m_character;
            m_grid[end_r][end_c].robotPtr = robot;
            std::cout << "  moving to (" << end_r << "," << end_c << ")" << std::endl;
            robot->move_to(end_r, end_c);
            return;
        }
        
    }
    m_grid[current_r][current_c].type = '.';
    m_grid[current_r][current_c].robotPtr = nullptr;
    m_grid[end_r][end_c].type = robot->m_character;
    m_grid[end_r][end_c].robotPtr = robot;

    std::cout << "  moving to (" << end_r << "," << end_c << ")" << std::endl;
    robot->move_to(end_r, end_c);
    return;
}

void Arena::display() const {
    std::cout << "   ";
    for (int c = 0; c < m_width; ++c) {
        std::cout << std::setw(3) << c;
    }
    std::cout << "\n";

    for (int r = 0; r < m_height; ++r) {
        std::cout << std::setw(2) << r << " ";

        for (int c = 0; c < m_width; ++c) {
            if (m_grid[r][c].robotPtr != nullptr) {
                std::cout << std::setw(2) << "R" << m_grid[r][c].robotPtr->m_character;
            } else {
                // Print '.', 'F', 'M', 'P', or 'X'
                std::cout << std::setw(3) << m_grid[r][c].type;
            }
        }
        std::cout << "\n";
    }
    std::cout << "\n---\n";

    for (RobotBase* robot : m_robots) {
        if (robot->get_health() <= 0) {
            std::cout << robot->m_name << " " << robot->m_character << " - is out\n";
            std::cout << "\n";
        }
    }
}