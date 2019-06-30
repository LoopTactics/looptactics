
float A[1024][1024];
float B[1024][1024];
float C[1024][1024];
float AA[1024][1024];
float BB[1024][1024];
float CC[1024][1024];
float alpha = 1.1;
float beta = 1.1;

int main(void) {


/*
#pragma scop
  for (int i = 0; i < 1024; i++)
    for (int j = 0; j < 1024; j++)
      {
        C[i][j] *= beta;
        for (int k = 0; k < 1024; ++k)
          C[i][j] += alpha * A[i][k] * B[k][j];
      }
#pragma endscop
*/


/*
#pragma scop
  for (int i = 0; i < 1024; i++)
    for (int j = 0; j < 1024; j++)
      {
        for (int k = 0; k < 1024; ++k)
          C[i][j] += alpha * A[i][k] * B[k][j];
      }
#pragma endscop
*/



#pragma scop
  for (int i = 0; i < 1024; i++)
    for (int j = 0; j < 1024; j++)
        C[i][j] *= beta;
        
  for (int i = 0; i < 1024; ++i)
    for (int k = 0; k < 1024; k++)
      for (int j = 0; j < 1024; j++)
        C[i][j] += alpha * A[i][k] * B[k][j];


  for (int i = 0; i < 1024; i++)
    for (int j = 0; j < 1024; j++)
        CC[i][j] *= beta;
        
  for (int i = 0; i < 1024; ++i)
    for (int k = 0; k < 1024; k++)
      for (int j = 0; j < 1024; j++)
        CC[i][j] += alpha * AA[i][k] * BB[k][j];
#pragma endscop






return 0;

}
