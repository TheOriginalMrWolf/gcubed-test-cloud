FROM mcr.microsoft.com/devcontainers/python:1-3.12
RUN apt-get update && export DEBIAN_FRONTEND=noninteractive && apt-get -y install \
  yacc \
  && rm -rf /var/lib/apt/lists/*
ENV PATH=$PATH:/workspaces/sym
