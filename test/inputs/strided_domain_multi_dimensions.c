void kernel(float **A, float **B, int n) {
  #pragma scop
  for (int i = 0; i < n; i+=1)
    for (int j = 0; j < n-1; j+=1)
      A[2*i][2*j] = B[i][2*j+2];
  #pragma endscop
}
