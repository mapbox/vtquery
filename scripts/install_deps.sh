#!/bin/bash

set -eu
set -o pipefail

function install() {
  mason install $1 $2
  mason link $1 $2
}

# setup mason
./scripts/setup.sh --config local.env
source local.env

install geometry 0.9.2
install variant 1.1.4
# install vtzero ...
# install protozero ...
# install spatial-algorithms ...
