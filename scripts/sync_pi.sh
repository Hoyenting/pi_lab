#!/bin/bash

PI_USER="tingpi"
PI_HOST="100.107.8.8"
PI_DIR="~/pi_lab"

rsync -av --delete \
  --exclude ".git" \
  --exclude "build" \
  --exclude "*.o" \
  --exclude "*.out" \
  --exclude ".DS_Store" \
  ./ "${PI_USER}@${PI_HOST}:${PI_DIR}/"