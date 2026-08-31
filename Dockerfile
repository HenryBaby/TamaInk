FROM python:3.12-slim-bookworm@sha256:0f5b26b9518d002b6173fd61daad821fa340635ebfec5bba471013f9ca114579

ENV PIP_DISABLE_PIP_VERSION_CHECK=1 \
    PLATFORMIO_CORE_DIR=/root/.platformio

RUN apt-get update \
    && apt-get install --no-install-recommends -y bash build-essential ca-certificates git \
    && rm -rf /var/lib/apt/lists/*

COPY requirements.txt /tmp/requirements.txt
RUN python -m pip install --no-cache-dir --requirement /tmp/requirements.txt

COPY docker/entrypoint.sh /usr/local/bin/tamaink-entrypoint
RUN chmod 0755 /usr/local/bin/tamaink-entrypoint

WORKDIR /workspace
ENTRYPOINT ["/usr/local/bin/tamaink-entrypoint"]
CMD ["/workspace/scripts/container-test.sh"]
