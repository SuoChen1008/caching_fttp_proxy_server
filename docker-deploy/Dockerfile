FROM gcc:10

# Install required packages
RUN apt-get update && apt-get install -y \
    g++ \
    make \
    uuid-dev \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir /var/log/erss

# Set working directory
WORKDIR /app

# Copy source code, Makefile and run script
COPY src/ /app/src/
COPY include/ /app/include/
COPY Makefile /app/
COPY run.sh /app/

RUN chmod +x /app/run.sh

# Set the entrypoint
ENTRYPOINT ["./run.sh"]
