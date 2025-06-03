This repository contains code and examples for ISTC 2025 ECC implementation challenge.

Quick start:
  - On an computer with Ubuntu linux, you can install g++ and make using "sudo apt install build-essential".
  - After that you can compile the example code by executing "make" in the "source" directory.
  - Try executing "run_test" to see what commands are available.
  - Try executing "run_test -o test_output.txt -f test_params.txt" to see how the testing process works.

The code is located in the course directory and the key files are:
  enc_dec.h
  enc_dec.cpp
  run_test.cpp
  test_params.txt

The file enc_dec.h is the stub that shows the calling structure for the test bed.
The file enc_dec.cpp contains an example implementation based on a very basic LDPC implementation.
The file run_test.cpp contains an initial version of the code that will call enc_dec.cpp to compute evaluation statistics.

Ancillary files include:
  ldpc.h
  ldpc.cpp
  argmin.h
  argmin.cpp

The files ldpc.h and ldpc.cpp provide a very basic LDPC implementation
The files argmin.h and argmin.cpp provide argument parsing support.

