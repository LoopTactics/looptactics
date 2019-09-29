void kernel(float *A, float *B, int n) {
  #pragma scop
  for (int i = 0; i < 100; i+=2)
    A[2*i + 1] = B[3*i] + B[i+5] + B[0];
  #pragma endscop
}
