#!/bin/bash

python3 ai_dueler_2023.py chess --num_games=100 ./our-11.sh ./benny_watts_player.sh > benny_vs_11.txt

python3 ai_dueler_2023.py chess --num_games=100 ./our-12.sh ./benny_watts_player.sh > benny_vs_12.txt