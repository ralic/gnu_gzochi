#!/bin/sh

GZOCHI_SERVER_HOSTNAME="localhost"
GZOCHI_SERVER_PORT=8001

NUM_CLIENTS=10
MESSAGES_PER_CLIENT=10


guile -e main -s client.scm localhost 8001 10 10 100
