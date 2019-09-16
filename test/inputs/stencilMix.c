void kernel_mix_stencils(double *A, double *B, double C[][400], double D[][400]) {

#pragma scop
  for (int t = 0; t < 400; t++) {
    for (int i = 1; i < 399; i++)
      B[i] = 0.33333 * (A[i - 1] + A[i] + A[i + 1]);
    for (int i = 1; i < 399; i++)
      A[i] = 0.33333 * (B[i - 1] + B[i] + B[i + 1]);
  }

  for (int t = 0; t < 400; t++) {
    for (int i = 1; i < 399; i++)
      for (int j = 1; j < 399; j++)
        D[i][j] = 0.125 * (C[i][j] + C[i][j-1] + C[i][1+j] + C[1+i][j] + C[i-1][j]);
    for (int i = 1; i < 399; i++)
      for (int j = 1; j < 399; j++)
        C[i][j] = 0.125 * (D[i][j] + D[i][j-1] + D[i][1+j] + D[1+i][j] + D[i-1][j]);
  }
#pragma endscop
}
