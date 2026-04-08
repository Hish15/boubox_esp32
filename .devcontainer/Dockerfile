FROM espressif/idf:release-v5.4
RUN apt-get update && apt-get install -y udev && rm -rf /var/lib/apt/lists/*
RUN echo "source /opt/esp/idf/export.sh" >> /root/.bashrc