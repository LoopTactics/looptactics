int tmp[1024][1024];
int A[1024][1024];
int B[1024][1024];
int alpha;

void kernel1mm() {
#pragma scop
  /* D := alpha*A*B*C + beta*D */
  for (int i = 0; i < 1024; i++)
    for (int j = 0; j < 1024; j++)
      for (int k = 0; k < 1024; k++) {
        if (k == 0)
          tmp[i][j] = 0;
        tmp[i][j] += alpha * A[i][k] * B[k][j];
      }
#pragma endscop
}
