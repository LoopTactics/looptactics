
#define NI 4000
#define NJ 4000
#define NK 4000
#define NL 4000
#define NM 4000

double A[NI][NK];
double B[NK][NJ];
double C[NJ][NM];
double D[NM][NL];
double E[NI][NJ];
double F[NJ][NL];
double G[NI][NL];

int main(void) {

  #pragma scop
  /* E := A*B */
  for (int i = 0; i < NI; i++)
    for (int j = 0; j < NJ; j++)
      {
	E[i][j] = 0;
	for (int k = 0; k < NK; ++k)
	  E[i][j] += A[i][k] * B[k][j];
      }
  /* F := C*D */
  for (int i = 0; i < NJ; i++)
    for (int j = 0; j < NL; j++)
      {
	F[i][j] = 0;
	for (int k = 0; k < NM; ++k)
	  F[i][j] += C[i][k] * D[k][j];
      }
  /* G := E*F */
  for (int i = 0; i < NI; i++)
    for (int j = 0; j < NL; j++)
      {
	G[i][j] = 0;
	for (int k = 0; k < NJ; ++k)
	  G[i][j] += E[i][k] * F[k][j];
      }
  #pragma endscop 
}
