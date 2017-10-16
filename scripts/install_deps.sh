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
install vtzero 556fac5
install protozero ccf6c39
install spatial-algorithms 2904283
install boost 1.65.1
