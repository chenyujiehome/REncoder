all:run_workload run_workload_simd test
run_workload: run_workload.cpp
	g++ -o run_workload run_workload.cpp -mcmodel=medium -march=native -mavx512f -O2
run_workload_simd: run_workload.cpp
	g++ -o run_workload_simd run_workload.cpp -mcmodel=medium -march=native -mavx512f -O2 -DUSE_SIMD
test: test.cpp
	g++ -o test test.cpp -mcmodel=medium -march=native -mavx512f -O2 -DUSE_SIMD