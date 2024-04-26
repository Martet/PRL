// Implementation of distributed Conway's Game of Life using openMPI, PRL project 2
// Author: Martin Zmitko (xzmitk01@stud.fit.vutbr.cz)
// Date: 2024-04-26
//
// An arbitrary number of processes (must be more than 1 and less than number of rows)
// is suported, as long as the number of rows is divisible by the number of processes.
// The field is split into chunks of rows for each process. In each iteration,
// the processes exchange their border rows and update their chunk of the field.
// Each processor computes numRows / numProcesses rows of the field.
// The grid is implemented as infinite, so the field wraps around the edges,
// only square grids are supported.

#include <iostream>
#include <fstream>
#include <vector>
#include "mpi.h"

typedef std::vector<int> row_t;
typedef std::vector<row_t> grid_t;

// Update the grid according to the rules of Conway's Game of Life
// takes a chunk of the grid including the two neightbouring rows and returns the updated chunk
// (wihtout the neightbouring rows)
grid_t updateGrid(grid_t grid) {
    const int gridSize = grid[0].size();
    const int chunkSize = grid.size() - 2;
    grid_t newGrid(chunkSize, row_t(gridSize, 0));
    for (int i = 1; i < chunkSize + 1; i++) {
        for (int j = 0; j < gridSize; j++) {
            int count = 0;
            for (int x = -1; x <= 1; x++) {
                for (int y = -1; y <= 1; y++) {
                    if (x == 0 && y == 0) {
                        continue;
                    }
                    // wrap around the grid, rows always available, columns wrap around
                    const int _j = j + y < 0 ? gridSize - 1 : (j + y) % gridSize;
                    count += grid[i + x][_j];
                }
            }
            if (grid[i][j] == 1) {
                newGrid[i - 1][j] = count == 2 || count == 3 ? 1 : 0;
            } else {
                newGrid[i - 1][j] = count == 3 ? 1 : 0;
            }
        }
    }
    return newGrid;
}

int main(int argc, char* argv[]) {
    MPI::Init();
    const int rank = MPI::COMM_WORLD.Get_rank();
    const int size = MPI::COMM_WORLD.Get_size();
    int chunkSize, gridSize;

    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input_file> <num_steps>" << std::endl;
        MPI::COMM_WORLD.Abort(1);
    }

    int numSteps;
    try {
        numSteps = std::stoi(argv[2]);
        if (numSteps < 0) {
            throw std::invalid_argument("Invalid number of steps.");
        }
    } catch (std::invalid_argument &e) {
        std::cerr << "Invalid number of steps." << std::endl;
        MPI::COMM_WORLD.Abort(1);
    }

    // The local grid of each process
    grid_t processGrid;
    
    // Process 0 reads the input file and distributes the grid to other processes
    if (rank == 0) {
        std::ifstream file(argv[1]);
        if (!file) {
            std::cerr << "Failed to open input file." << std::endl;
            MPI::COMM_WORLD.Abort(1);
        }

        grid_t grid;
        std::string line;
        while (std::getline(file, line)) {
            row_t row;
            for (char c : line) {
                row.push_back(c - '0');
            }
            grid.push_back(row);
        }
        file.close();

        gridSize = grid.size();
        chunkSize = gridSize / size;
        if (gridSize == 0) {
            std::cerr << "Empty grid." << std::endl;
            MPI::COMM_WORLD.Abort(1);
        }
        for (int i = 0; i < gridSize; i++) {
            if (grid[i].size() != gridSize) {
                std::cerr << "The grid must be square." << std::endl;
                MPI::COMM_WORLD.Abort(1);
            }
        }

        // Initialize the local grid of process 0
        for (int i = 0; i < chunkSize; i++) {
            processGrid.push_back(grid[i]);
        }

        // Send chunks of the grid to other processes
        for (int i = 1; i < size; i++) {
            // First, other processes need to know the size of the grid and their chunk
            int msg[2] = {chunkSize, (int)grid[0].size()};
            MPI::COMM_WORLD.Send(&msg, 2, MPI::INT, i, 0);

            // Then, send the chunks of the grid
            for (int j = chunkSize * i; j < chunkSize * i + chunkSize; j++) {
                MPI::COMM_WORLD.Send(grid[j].data(), gridSize, MPI::INT, i, 0);
            }
        }
    }
    else { // Other processes receive the grid from process 0 
        int msg[2];
        MPI::COMM_WORLD.Recv(&msg, 2, MPI::INT, 0, 0);
        chunkSize = msg[0];
        gridSize = msg[1];
        int *row = new int[gridSize];
        for (int i = 0; i < chunkSize; i++) {
            MPI::COMM_WORLD.Recv(row, gridSize, MPI::INT, 0, 0);
            processGrid.push_back(row_t(row, row + gridSize));
        }
        delete[] row;
    }

    // Run the simulation for each process
    int *row = new int[gridSize];
    for (int i = 0; i < numSteps; i++) {
        // Send the border rows to the neighbouring processes
        MPI::COMM_WORLD.Send(processGrid[chunkSize - 1].data(), gridSize, MPI::INT, (rank + 1) % size, 0);
        MPI::COMM_WORLD.Send(processGrid[0].data(), gridSize, MPI::INT, rank == 0 ? size - 1 : rank - 1, 0);

        // Receive the border rows from the neighbouring processes
        MPI::COMM_WORLD.Recv(row, gridSize, MPI::INT, rank == 0 ? size - 1 : rank - 1, 0);
        processGrid.insert(processGrid.begin(), row_t(row, row + gridSize));
        MPI::COMM_WORLD.Recv(row, gridSize, MPI::INT, (rank + 1) % size, 0);
        processGrid.push_back(row_t(row, row + gridSize));
        
        // Update the grid
        processGrid = updateGrid(processGrid);
    }
    delete[] row;

    // Print the final state of the grid
    // Barriers are used to ensure that the output is printed in the correct order,
    // however, the outputs of different processes may be interleaved as this is
    // a limitation of multiple processes writing to the same output stream with
    // no option to force a buffer flush.
    for (int i = 0; i < rank; i++) {
        MPI::COMM_WORLD.Barrier();
    }
    for (int i = 0; i < chunkSize; i++) {
        std::cout << rank << ": ";
        for (int j = 0; j < gridSize; j++) {
            std::cout << processGrid[i][j];
        }
        std::cout << std::endl;
    }
    for (int i = rank + 1; i < size; i++) {
        MPI::COMM_WORLD.Barrier();
    }

    MPI::Finalize();
    return 0;
}
