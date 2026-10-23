ARG BUILD_FROM
FROM $BUILD_FROM

ENV LANG=C.UTF-8 \
    PIP_BREAK_SYSTEM_PACKAGES=1 \
    PLATFORMIO_CORE_DIR=/data/.platformio

# Toolchains: git + make + sdcc (CC2510), python3 + pip (PlatformIO/esptool/CCLib),
# jq (Options lesen). PlatformIO zieht ESP-Toolchain/Framework beim ersten Build.
RUN apt-get update && apt-get install -y --no-install-recommends \
      git make sdcc jq python3 python3-pip ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN pip3 install --no-cache-dir platformio esptool pyserial flask

COPY run.sh /run.sh
COPY app /opt/app
RUN chmod a+x /run.sh

CMD [ "/run.sh" ]
