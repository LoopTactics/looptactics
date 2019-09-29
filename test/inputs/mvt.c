
float A[1024][1024];
float x[1024];
float X[1024];
float y[1024];
float Y[1024];

int main(void) {

#pragma scop
  for (int i = 0; i < 1024; i++)
    for (int j = 0; j < 1024; j++)
      x[i] = x[i] + A[i][j] * y[j];
  for (int i = 0; i < 1024; i++)
    for (int j = 0; j < 1024; j++)
      X[i] = X[i] + A[j][i] * Y[j];
#pragma endscop
return 0;
}
