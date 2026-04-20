# Usa un'immagine base con gli strumenti di compilazione C (gcc, make, ecc.)
FROM ubuntu:latest

RUN apt update && DEBIAN_FRONTEND=noninteractive apt install -y gcc libcjson-dev make 
# RUN apk update && apk add --no-cache gcc make cjson-dev g++ clang llvm compiler-rt

# Imposta la directory di lavoro
WORKDIR /app

# Copia tutti i file sorgente (includi solo i file necessari per ottimizzare la cache Docker)
COPY makefile .
COPY *.c ./
COPY *.h ./

EXPOSE 5200

# Compila l'applicazione usando il Makefile
RUN make --debug=v

# Comando di avvio del server
CMD ["./server.out", "5200"]