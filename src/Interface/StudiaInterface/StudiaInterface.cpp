#include <Interface/StudiaInterface/StudiaInterface.hpp>
#include <iostream>
#include <sstream>    

void StudiaInterface::run(Bot& bot)
{
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);
    
    std::cout << "RDY" << std::endl;

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string command;
        ss >> command;

        if (command == "UGO") {
            double time_for_move, time_remaining;
            ss >> time_for_move >> time_remaining;
            std::string my_move = bot.make_first_move(time_for_move, time_remaining);
            std::cout << "IDO " << my_move << std::endl;
            
        } else if (command == "HEDID") {
            std::string time_for_move_str, time_remaining_str;
            ss >> time_for_move_str >> time_remaining_str;
            
            std::string opponent_move;
            std::getline(ss, opponent_move);
            
            if (!opponent_move.empty() && opponent_move.back() == '\r') {
                opponent_move.pop_back();
            }
            if (!opponent_move.empty() && opponent_move[0] == ' ') {
                opponent_move = opponent_move.substr(1);
            }

            double t_move = 0.0, t_left = 0.0;
            try {
                t_move = stod(time_for_move_str);
                t_left = stod(time_remaining_str);
            } catch (...) {  }

            std::string my_move = bot.make_move(t_move, t_left, opponent_move);
            std::cout << "IDO " << my_move << std::endl;
        } else if (command == "ONEMORE") {
            bot.reset();
            std::cout << "RDY" << std::endl;
            
        } else if (command == "BYE") {
            // std::cerr << "[DEBUG] Zamykanie bota..." << std::endl;
            break;
        }
    }
};