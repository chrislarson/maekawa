#!/bin/bash

NUM_HOSTS=${1:-9}

if [ $NUM_HOSTS -lt 1 ] || [ $NUM_HOSTS -gt 9 ]; then
    echo "Error: NUM_HOSTS must be between 1 and 9"
    exit 1
fi

docker compose down --remove-orphans

docker network rm maekawa_cluster_net

export NUM_HOSTS
docker compose up -d $(for i in $(seq 0 $((NUM_HOSTS-1))); do echo -n "host$i "; done)

# Show the logs
docker compose logs -f