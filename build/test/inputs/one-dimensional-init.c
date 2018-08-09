void f(float A[]) {
  #pragma scop
  for (int i = 0; i < 1024; i++)
    A[i] = 0;
  #pragma endscop
}
