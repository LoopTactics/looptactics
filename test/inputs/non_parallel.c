void parallel(double *A) {

#pragma scop

  for (int i = 1; i < 1024; i++) {
    A[i] = A[i - 1] + 10;
  }

#pragma endscop
}
