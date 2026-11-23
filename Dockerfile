# Use locally cached Debian Bookworm base image (avoids credential helper lookups over SSH)
FROM node:22-slim

# Install C++17 compiler (g++) and OpenSSL development headers (libssl-dev)
RUN apt-get update && apt-get install -y --no-install-recommends g++ libssl-dev && rm -rf /var/lib/apt/lists/*

# Work at /app where common/, phase1-paper-setting/, phase2-distribution/, phase3-exam-centre/, server/, frontend/ live
WORKDIR /app

# Copy the repository and pre-built frontend/dist into the container
COPY . /app

# Compile the full 3-phase C++17 HTTP server and CLI executables with OpenSSL (-lcrypto)
RUN g++ -O2 -std=c++17 -pthread -Icommon -Iphase1-paper-setting -Iphase2-distribution -Iphase3-exam-centre -Iserver server/server.cpp -lcrypto -o secure_cbt_server \
 && g++ -O2 -std=c++17 -Icommon -Iphase1-paper-setting phase1-paper-setting/main.cpp -lcrypto -o secure_cbt \
 && g++ -O2 -std=c++17 -Icommon -Iphase2-distribution phase2-distribution/main.cpp -lcrypto -o secure_cbt_phase2 \
 && g++ -O2 -std=c++17 -Icommon -Iphase1-paper-setting -Iphase2-distribution -Iphase3-exam-centre phase3-exam-centre/main.cpp -lcrypto -o secure_cbt_phase3

# Default port for Dell server deployment
ENV PORT=8090
EXPOSE 8090

# Start the full 3-phase C++ server
CMD ["./secure_cbt_server"]
