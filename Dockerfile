FROM gcc:4.9

COPY . /src/grep_app

WORKDIR /src/grep_app

RUN gcc -o grep -pthread mp0_rachel.c 

CMD ["./grep"]
