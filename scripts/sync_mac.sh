#!/bin/bash

MAC_USER="hoting"
MAC_HOST="100.102.179.21"
MAC_DIR="~/pi_lab"

rsync -av --delete \
  --exclude ".git" \
  --exclude "build" \
  --exclude "*.o" \
  --exclude "*.out" \
  --exclude ".DS_Store" \
  ./ "${MAC_USER}@${MAC_HOST}:${MAC_DIR}/"
