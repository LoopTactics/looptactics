
float A[1024][1024];
float y[1024];
float x[1024];
float tmp[1024];

int main(void) {

#pragma scop
  for (int i = 0; i < 1024; i++)
    y[i] = 0;
    for (int i = 0; i < 1024; i++) {
      tmp[i] = 0;
      for (int j = 0; j < 1024; j++) {
        tmp[i] = tmp[i] + A[i][j] * x[j];
      }
      for (int j = 0; j < 1024; j++) {
        y[j] = y[j] + A[i][j] * tmp[i];
      }
    }
#pragma endscop
return 0;
}
