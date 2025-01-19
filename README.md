A repository of code to investigate the derivation of the Boltzmann distribution through simulation.

## Compilation and Execution Instructions

To compile the program, use the following command:

```bash
g++ -std=c++17 -fsanitize=address -g disk_sim.cpp -o disk_sim \
    -lsfml-graphics -lsfml-window -lsfml-system

To run:
```bash
./disk_sim