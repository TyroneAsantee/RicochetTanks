<img width="793" height="623" alt="RicTankMENU" src="https://github.com/user-attachments/assets/13e5305a-5907-4571-b7d4-6fec3663eefd" />
<img width="798" height="626" alt="RicTankIP" src="https://github.com/user-attachments/assets/5fc89554-b56d-4d18-bfea-75733bc56f15" />
<img width="796" height="629" alt="RicTankGame" src="https://github.com/user-attachments/assets/3127d5ce-fe41-4bc6-9ba3-b467d896e876" />

# RicochetTanks

## Description
RicochetTanks is a network-based multiplayer game developed in C using SDL2.  
Up to four players can control a tank on an arena, choose colors, and shoot projectiles that bounce off the walls. The project was developed as part of a computer engineering course at KTH, focusing on real-time rendering, client-server communication, and gameplay mechanics.

## Technologies and Tools
- Programming language: C
- Libraries: SDL2, SDL_image
- Tools: Git, Make, GCC
- Architecture: Client-Server

## Key Learnings
- Network programming and client-server architecture
- Handling game graphics and real-time updates with SDL2
- Git workflow in a team (branches, pull requests, code review)
- Structuring projects into multiple modules (client, server, lib)

## Project Structure
- Project Structure
- client/ – client code (player side)
- server/ – server code (match handling)
- lib/ – shared resources, headers, game assets
- resources/ – images for tanks, background, etc.

## About
- About
- Developed by: Tyrone Asante + project team (KTH)
- Program: Bachelor of Science in Computer Engineering, KTH Royal Institute of Technology

## How to Run
- START SERVER FIRST
- cd ../server
- make
- ./server
- THEN GAME
```bash
git clone https://github.com/TyroneAsantee/RicochetTanks.git
cd RicochetTanks/client
make
./client




