// Implementation of pipeline merge sort, PRL project 1
// Author: Martin Zmitko (xzmitk01@stud.fit.vutbr.cz)
// Date: 2024-04-03

#include "mpi.h"
#include <iostream>
#include <iterator>
#include <fstream>
#include <vector>
#include <queue>
#include <cmath>
#include <cstdlib>

enum Queue {QUEUE_A, QUEUE_B};

int main(int argc, char *argv[]){
    MPI::Init();
    int rank = MPI::COMM_WORLD.Get_rank();
    int size = MPI::COMM_WORLD.Get_size();
    // Process 0 reads numbers from file and sends them to process 1
    if(rank == 0){
       // Open file and read numbers
        std::ifstream file("numbers", std::ios::binary);
        std::vector<unsigned char> numbers((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        if(numbers.size() == 0){
            std::cerr << "Process 0: No numbers in file, aborting" << std::endl;
            MPI::COMM_WORLD.Abort(1);
        }
        size_t numbers_size = numbers.size();
        size_t required_size = std::ceil(std::log2(numbers_size) + 1);
        if(size < required_size){
            std::cerr << "Process 0: Not enough processes to sort numbers (" << required_size << " required for " << numbers_size << " numbers), aborting" << std::endl;
            MPI::COMM_WORLD.Abort(1);
        }

        // Print input numbers
        std::cout << (int)numbers[0];;
        for(int i = 1; i < numbers.size(); i++){
            std::cout << " " << (int)numbers[i];
        }
        std::cout << std::endl;

        // Send out numbers to process 1
        int queue = QUEUE_A;
        for(auto number : numbers){
            MPI::COMM_WORLD.Send(&number, 1, MPI::UNSIGNED_CHAR, 1, queue);
            queue = queue == QUEUE_A ? QUEUE_B : QUEUE_A;
        }
        // Always send empty message to signal end of numbers
        MPI::COMM_WORLD.Send(nullptr, 0, MPI::UNSIGNED_CHAR, 1, queue);
    }
    else { // Processes 1 to size - 1
        std::queue<unsigned char> queue_A, queue_B;
        int sending_queue = QUEUE_A;
        const int receiving_limit = std::pow(2, rank - 1);
        const int sending_limit = std::pow(2, rank);
        int sent = 0;
        int read_A = 0, read_B = 0;
        bool receiving = true;
        bool sending = false;
        while(true){
            // Receive numbers from previous process
            if(receiving){
                unsigned char number;
                MPI::Status status;
                MPI::COMM_WORLD.Recv(&number, 1, MPI::UNSIGNED_CHAR, rank - 1, MPI::ANY_TAG, status);
                int received = status.Get_count(MPI::UNSIGNED_CHAR);
                if(received){
                    auto *queue = status.Get_tag() == QUEUE_A ? &queue_A : &queue_B;
                    queue->push(number);
                }
                else{
                    receiving = false;
                    sending = true; // If not enough numbers arrived, send anyway
                }
            }
            // Set sending flag if limit of 2^(i-1) and 1 items in queues is reached
            if(!sending && queue_A.size() >= receiving_limit && queue_B.size() > 0){
                sending = true;
            }

            // Send numbers to next process
            if(sending){
                int number = 256; // Larger than any number, can't show in output
                if(!queue_A.empty() && read_A < receiving_limit){
                    number = queue_A.front();
                }
                if(!queue_B.empty() && queue_B.front() < number && read_B < receiving_limit){
                    number = queue_B.front();
                    queue_B.pop();
                    read_B++;
                }
                else if((queue_A.empty() || read_A >= receiving_limit) && read_B < receiving_limit && !queue_B.empty()){
                    number = queue_B.front();
                    queue_B.pop();
                    read_B++;
                }
                else{
                    queue_A.pop();
                    read_A++;
                }

                // Reset read counters if sequence is complete
                if(read_A >= receiving_limit && read_B >= receiving_limit){
                    read_A = 0;
                    read_B = 0;
                }

                if(rank == size - 1){
                    std::cout << (int)number << std::endl;
                }
                else{
                    MPI::COMM_WORLD.Send(&number, 1, MPI::UNSIGNED_CHAR, rank + 1, sending_queue);
                }

                sent++;
                if(sent >= sending_limit){
                   sending_queue = sending_queue == QUEUE_A ? QUEUE_B : QUEUE_A;
                   sent = 0;
                }
            }

            // Quit if no more numbers to receive and no more numbers to send
            if(!receiving && queue_A.empty() && queue_B.empty()){
                if(rank != size - 1){
                    MPI::COMM_WORLD.Send(nullptr, 0, MPI::UNSIGNED_CHAR, rank + 1, sending_queue);
                }
                break;
            }
        }
    }

    MPI::Finalize();
    return 0;
}
