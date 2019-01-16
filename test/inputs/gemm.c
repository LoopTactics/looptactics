
float A[1024][1024];
float B[1024][1024];
float C[1024][1024];
float alpha = 1.1;
float beta = 1.1;

//int ni = 1024;
//int nj = 1024;
//int nk = 1024;

int main(void) {

#pragma scop
  /* C := alpha*A*B + beta*C */
  for (int i = 0; i < 1024; i++)
    for (int j = 0; j < 1024; j++)
      {
        C[i][j] *= beta;
        for (int k = 0; k < 1024; ++k)
          C[i][j] += alpha * A[i][k] * B[k][j];
      }
#pragma endscop

return 0;

}
