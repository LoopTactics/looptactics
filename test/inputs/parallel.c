void parallel(double *A) {

#pragma scop

  for (int i = 0; i < 1024; i++) {
    A[i] = A[i] + 10;
  }

#pragma endscop
}
