# Stage 1: Build the application
FROM debian:bookworm-slim AS build

# Install build dependencies
RUN apt-get update && apt-get install -y \
    g++ \
    cmake \
    make \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Set the working directory
WORKDIR /app

# Copy the source code into the container
COPY . .

# Build the application
RUN cmake . && make

# Stage 2: Create the final image with the built application
FROM debian:bookworm-slim

# Copy the built binary from the build stage
COPY --from=build /app/app/app /app/app/app

# Set the working directory
WORKDIR /app

# Expose the ports for communication
EXPOSE 9100-9110

# Command to run the application
CMD ["./app/app"]