FROM ubuntu:22.04
RUN apt-get update && apt-get install -y build-essential cmake python3 python3-pip python3-matplotlib python3-numpy
WORKDIR /app
COPY . /app
CMD ["/bin/bash"]
