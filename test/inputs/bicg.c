
float A[1024][1024];
float s[1024];
float r[1024];
float q[1024];
float p[1024];

int main(void) {

#pragma scop
  for (int i = 0; i < 1024; i++)
    s[i] = 0;
    for (int i = 0; i < 1024; i++) {
      q[i] = 0;
      for (int j = 0; j < 1024; j++) {
        s[j] = s[j] + r[i] * A[i][j];
        q[i] = q[i] + A[i][j] * p[j];
      }
    }
#pragma endscop
return 0;
}
