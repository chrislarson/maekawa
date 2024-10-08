## maekawa_v1 Implementation

This project implements vector clocks and Maekawa's mutual exclusion algorithm for distributed systems.

## Design

1. `CDistributedMutex`: Implementation of Maekawa's algorithm using vector clocks.
2. `NetworkLayer`: Handles network communication between processes.
3. `Logger`: Logging functionality for debugging and monitoring.

## Assumptions

1. No process crashes or network outages.
2. Number of processes is fixed at startup and doesn't change during execution.
3. Only one process runs on each machine.

## Building the Project

To build the project:

1. Requirements: C++17 and cmake.
2. Navigate to the project root directory.
3. Run the following commands:

   

```
   mkdir build
   cd build
   cmake ..
   make
   ```

The generated executable will be in the `build/app` directory.

## Running the Project

1. Requirements: Docker and Docker Compose.
2. Build the Docker containers. From root of repo run: ```docker compose build```
3. Launch the Docker containers using the shell script. From root of repo run: ```./start_containers.sh {NUM_HOSTS}``` where NUM_HOSTS is 1-9.

## Logging

Logs are written to standard output. Log level can be adjusted in the docker compose file using the `LOG_LEVEL` env variable.

## Known Limitations

* Deadlocks are possible due to the lack of liveness in Maekawa v1.
