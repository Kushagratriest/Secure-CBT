# Lightweight Linux container with GCC/G++ C++17 compiler
FROM gcc:13-bookworm

# Work inside the "secure cbt" folder where common/, phase1-paper-setting/, server/, and frontend/ live
WORKDIR "/app/secure cbt"

# Copy the repository into the container
COPY . /app

# Compile the C++ HTTP server with C++17 and pthread support
RUN g++ -O2 -std=c++17 -pthread -Icommon -Iphase1-paper-setting -Iserver server/server.cpp -o secure_cbt_server

# Default port (Render overrides PORT automatically; Hugging Face Spaces uses 7860)
ENV PORT=7860
EXPOSE 7860

# Start the C++ server
CMD ["./secure_cbt_server"]
