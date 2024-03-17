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
        if(size < std::log2(numbers.size())){
            std::cerr << "Process 0: Not enough processes to sort numbers, aborting" << std::endl;
            MPI::COMM_WORLD.Abort(1);
        }

        // Print input numbers
        std::cout << (int)numbers[0];;
        for(int i = 1; i < numbers.size(); i++){
            std::cout << " " << (int)numbers[i];
        }
        std::cout << std::endl;
        MPI::COMM_WORLD.Barrier();

        // Send out numbers to process 1
        int queue = QUEUE_A;
        for(auto number : numbers){
            MPI::COMM_WORLD.Send(&number, 1, MPI::UNSIGNED_CHAR, 1, queue);
            queue = queue == QUEUE_A ? QUEUE_B : QUEUE_A;
        }
        MPI::COMM_WORLD.Send(nullptr, 0, MPI::UNSIGNED_CHAR, 1, queue);
    }
    else { // Processes 1 to size - 1
        MPI::COMM_WORLD.Barrier();
        std::queue<unsigned char> queue_A, queue_B;
        int sending_queue = QUEUE_A;
        const int sending_limit = std::pow(2, rank - 1);
        const int receiving_limit = std::ceil(std::pow(2, rank - 2));
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
                    //printf("Process %d received %d in queue %s", rank, (int)number, status.Get_tag() == QUEUE_A ? "A\n" : "B\n");
                }
                else{
                    receiving = false;
                    //printf("Process %d stopped receiving\n", rank);
                }
            }
            // Set sending flag if limit of 2^(i-1) and 1 items in queues is reached
            if(!sending && queue_A.size() >= sending_limit && queue_B.size() > 0){
                sending = true;
            }

            // Send numbers to next process
            if(sending){
                //printf("Process %d A=%d B=%d ", rank, queue_A.front(), queue_B.front());
                int number = 256;
                if(!queue_A.empty() && read_A < receiving_limit){
                    number = queue_A.front();
                }
                if(!queue_B.empty() && queue_B.front() < number && read_B < receiving_limit){
                    number = queue_B.front();
                    queue_B.pop();
                    read_B++;
                }
                else if((queue_A.empty() || read_A < receiving_limit) && read_B < receiving_limit){
                    number = queue_B.front();
                    queue_B.pop();
                    read_B++;
                }
                else{
                    queue_A.pop();
                    read_A++;
                }
                if(read_A >= receiving_limit && read_B >= receiving_limit){
                    read_A = 0;
                    read_B = 0;
                }
                //printf("picked %d\n", number);
                if(rank == size - 1){
                    std::cout << (int)number << std::endl;
                }
                else{
                    MPI::COMM_WORLD.Send(&number, 1, MPI::UNSIGNED_CHAR, rank + 1, sending_queue);
                    //printf("Process %d sent %d\n", rank, (int)number);
                }

                if(sent >= sending_limit){
                   sending_queue = sending_queue == QUEUE_A ? QUEUE_B : QUEUE_A;
                   sent = 0;
                }
                sent++;
            }

            // Quit if no more numbers to receive and no more numbers to send
            if(!receiving && queue_A.empty() && queue_B.empty()){
                if(rank != size - 1){
                    MPI::COMM_WORLD.Send(nullptr, 0, MPI::UNSIGNED_CHAR, rank + 1, sending_queue);
                }
                //printf("Process %d stopped sending\n", rank);
                break;
            }
        }
    }


    MPI::Finalize();
    return 0;
}
