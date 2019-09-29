// Test that we bind the same schedule dimension to the same placeholder. 

void kernel_stencils(double *A, double *B, double C[][400], double D[][400]) {

#pragma scop
  for (int t = 0; t < 400; t++) {
    for (int i = 2; i < 398; i++)
      B[i] = 0.33333 * (A[i - 2] + A[i - 1] + A[i] + A[i + 1] + A[i + 2]);
  }
#pragma endscop
}
