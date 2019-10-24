void kernel_jacobi_1d(int tsteps, int n, double *A, double *B) {

#pragma scop
  for (int t = 0; t < 400; t++) {
    for (int i = 1; i < 399; i++)
      B[i] = 0.33333 * (A[i - 1] + A[i] + A[i + 1]);
    for (int i = 1; i < 399; i++)
      A[i] = 0.33333 * (B[i - 1] + B[i] + B[i + 1]);
  }
#pragma endscop
}
