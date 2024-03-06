export TOPDIR=$(pwd)
make
chmod +x ./benchmarks/*
python3 benchmarks/runall.py ./benchmarks