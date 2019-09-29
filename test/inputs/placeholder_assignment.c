// Test that we bind the same schedule dimension to the same placeholder. 

void kernel_stencils(double *A, double *B, double C[][400], double D[][400]) {

#pragma scop
  for (int t = 0; t < 400; t++) {
    for (int i = 1; i < 399; i++)
      B[i] = 0.33333 * (A[i - 1] + A[i] + A[i + 1]);
    for (int i = 1; i < 399; i++)
      for (int j = 0; j < 5; j++)
        A[j] = 0.33333 * (B[i - 1] + B[i] + B[i + 1]);
  }
#pragma endscop
}
