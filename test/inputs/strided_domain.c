void kernel(float *A, float *B, int n){
#pragma scop
  for (int i = 0; i < n; i += 3) {
    A[i] = B[i];
  }
#pragma endscop
}
