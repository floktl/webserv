# Nutze das Basis-Image Ubuntu
FROM ubuntu:latest

# Aktualisiere Paketliste und installiere Valgrind
RUN apt-get update && \
    apt-get install -y valgrind make gcc g++ bash && \
    apt-get clean

# Setze das Arbeitsverzeichnis
WORKDIR /app

# Kopiere alle Dateien ins Image
COPY ./ /app

# Setze bash als Standardbefehl
CMD ["bash"]
