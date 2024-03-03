export TOPDIR=$(pwd)
make
python3 benchmarks/runall.py ./benchmarks