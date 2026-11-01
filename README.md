Akshita Sharma 217-A 26BAI10093
🟡 Pac-Man Code: Full Breakdown
📦 1. Libraries Used
Header	🎯 Purpose
windows.h	🖥️ Console API: colours, cursor, fast screen drawing, timers
conio.h	⌨️ Instant keyboard input (_kbhit, _getch)
stdio.h / stdlib.h	🔤 Text formatting, rand(), system()
string.h	📋 Copying the maze (strcpy)
time.h	🎲 Seeding the random number generator
⚙️ 2. Constants and Settings
📐 MW / MH: maze size, 19 x 21 cells
📍 OX / OY: where the maze is placed on screen
🖼️ SW / SH: screen buffer size, 64 x 24
🧱 WALL_ATTR: wall colour (bright blue)
⏱️ PAC_DELAY: milliseconds between Pac-Man’s moves (130 ms)
🗺️ 3. The Maze (LEVEL_MAP)

The maze is an array of text strings, so it is easy to edit:

Symbol	Meaning
#	🧱 Wall
.	🟡 Small pellet
o	⭐ Power pellet
-	🚪 Ghost door (only ghosts can pass)
G	👻 Ghost house
P	😮 Pac-Man start
 	⬜ Empty path
🧩 4. Data Types
🧭 Direction enum: UP, LEFT, DOWN, RIGHT, with DX[] / DY[] lookup tables for movement
👻 Ghost modes: NORMAL, FRIGHT (blue), EATEN (eyes)
🎬 Game states: READY, PLAY, DYING, CLEAR, OVER
👾 Ghost struct: position, direction, mode, whether it is inside the house, move timer, and release time
🎨 Ghost colours: red, magenta, cyan, orange
🏁 Scatter corners: the corner each ghost retreats to
⏳ Release times: ghosts leave the house at 0, 2, 5 and 8 seconds
🧠 5. Core Systems
🚶 Movement and Walls
✅ walkable() checks whether a cell can be entered
🌀 Uses modulo (% MW) to create the wrap-around tunnel
🚪 The ghost door only opens for ghosts
🔍 BFS Pathfinding
🗺️ build_home_dist() runs a breadth-first search from the ghost house
📏 It stores the shortest distance from every cell back home
👀 Eaten ghosts (eyes) follow this map to return quickly
🔄 Level Loading
📥 load_level() copies the map, counts pellets, and removes the P and G markers
🔁 reset_positions() places Pac-Man and the ghosts back at their starts and shows READY!
👻 6. Ghost AI
🎯 Targeting (ghost_target)
Ghost	🎨	Strategy
🔴 Red	Blinky	Chases Pac-Man’s current tile
🩷 Pink	Pinky	Aims 4 cells ahead of Pac-Man
🔵 Cyan	Inky	Mirrors red’s position around a point ahead of Pac-Man (flanking)
🟠 Orange	Clyde	Chases when far (more than 8 cells), retreats to his corner when close
🔀 Scatter vs Chase
🏝️ 7 seconds heading to home corners
🏹 20 seconds hunting Pac-Man
🔁 The cycle repeats
🧭 Decision Making (step_ghost)
🟢 Normal: greedy pathing, pick the legal direction that minimises distance to the target (never reversing)
🔵 Frightened: pick a random legal direction
👀 Eaten: follow the BFS map straight home
🚪 In the house: head toward the exit tile
🔙 Dead end: turn around
✨ Eyes reaching home revive into a normal ghost
🏃 Speed (ghost_delay)
⚡ Base speed gets faster each level (down to a minimum)
🐢 Frightened ghosts are slower
🚀 Eyes move very fast
😮 7. Pac-Man Logic
🕹️ Buffered turning: your next direction is stored in want and applied at the first opening
🟡 Eating: pellets give 10 points, power pellets give 50
⭐ Power pellet (frighten()): turns ghosts blue, flips their direction, and starts a timer
⏳ Power-up duration shrinks each level (7 s down to a minimum of 2.5 s)
💥 Collisions (check_collisions):
🔵 Touch a blue ghost: score 200 → 400 → 800 → 1600
🔴 Touch a normal ghost: lose a life
😋 Mouth animation: Pac-Man alternates between open and closed
🎬 8. Game State Machine (in main)
State	🎞️ What happens
⏳ READY	Shows “READY!” for 2 seconds, then starts the timers
▶️ PLAY	Moves Pac-Man and each ghost on independent timers, checks collisions and win condition
💀 DYING	Pac-Man blinks for 1.5 seconds, then resets or ends the game
🎉 CLEAR	“LEVEL CLEAR!” for 2 seconds, then the next level loads
🏁 OVER	Shows “GAME OVER” and waits for a key press
🎨 9. Rendering System
🖼️ Off-screen buffer (CHAR_INFO scr[]): the whole frame is built in memory first
⚡ present() pushes it to the console in a single WriteConsoleOutputA call, so there is no flicker
✍️ put_str() writes text with a colour attribute at any position
🟦 Each maze cell is 2 characters wide so it looks square
✨ Visual effects:
💫 Power pellets blink
🔵 Frightened ghosts flash red and white during the last 2 seconds
👀 Eaten ghosts appear as just the eyes
💀 Pac-Man blinks when dying
📊 Side panel shows score, level, lives, controls, and hints
⌨️ 10. Controls and Input
⬆️⬇️⬅️➡️ Arrow keys (read as two-byte codes) or W A S D
⏸️ P pauses (shift_timers() keeps all timers fair after resuming)
🚪 Q quits
📈 11. Difficulty Scaling
Per level	Effect
👻 Ghost delay	🔽 10 ms faster each level (minimum 80 ms)
⭐ Power-up time	🔽 800 ms shorter each level (minimum 2.5 s)
🧾 12. Quick Feature Summary
✅ 4 ghosts with unique AI 🧠
✅ Scatter and chase phases 🔄
✅ Power pellets and ghost-eating combo 💥
✅ BFS pathfinding for returning eyes 🗺️
✅ Wrap-around tunnel 🌀
✅ Ghost house with staggered releases 🚪
✅ Buffered turning for smooth control 🕹️
✅ 3 lives, levels, and increasing difficulty 📈
✅ Pause, quit, and colourful UI 🎨
✅ Flicker-free rendering ⚡


