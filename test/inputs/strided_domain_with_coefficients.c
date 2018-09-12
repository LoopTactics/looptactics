void kernel(float *A, float *B, int n) {
  #pragma scop
  for (int i = 0; i < n; i+=2)
    A[2*i + 1] = B[3*i] + B[3*i+5] + B[0];
  #pragma endscop
}
