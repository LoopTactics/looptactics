
#define NI 4000
#define NJ 4000
#define NK 4000
#define NL 4000

double alpha = 1.0;
double beta = 1.0;
double gamma = 5.9;

double tmp[NI][NJ];
double tmptmp[NI][NJ];
double A[NI][NK];
double AA[NI][NK];
double B[NK][NJ];
double BB[NK][NJ];
double C[NL][NJ];
double CC[NL][NJ];
double D[NI][NL];
double DD[NI][NL];

int main(void) {

  #pragma scop
  /* D := alpha*A*B*C + beta*D */
  for (int i = 0; i < NI; i++)
    for (int j = 0; j < NJ; j++)
      {
	tmp[i][j] *= gamma;
	for (int k = 0; k < NK; ++k) {
	  tmp[i][j] += beta * A[i][k] * B[k][j];
    tmptmp[i][j] += gamma * AA[i][k] * BB[k][j];
  }
      }
  for (int i = 0; i < NI; i++)
    for (int j = 0; j < NL; j++)
      {
	D[i][j] = 0;
	for (int k = 0; k < NJ; ++k) {
	  D[i][j] += beta * tmp[i][k] * C[k][j];
    DD[i][j] += gamma * tmptmp[i][k] * CC[k][j];
  }
      }
  #pragma endscop
}
