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

install geometry 96d3505
install variant 1.1.4
install vtzero 533b811
install protozero 1.6.0
install spatial-algorithms cdda174
install boost 1.65.1
install cheap-ruler 2.5.3
install vector-tile f4728da
