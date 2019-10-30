#define _PB_N 1024

float A[_PB_N][_PB_N];
float u1[_PB_N];
float v1[_PB_N];
float u2[_PB_N];
float v2[_PB_N];
float x[_PB_N];
float y[_PB_N];
float z[_PB_N];
float w[_PB_N];

float alpha = 1.0;
float beta = 1.0;

int main() {

#pragma scop
for (int i = 0; i < _PB_N; i++)
  for (int j = 0; j < _PB_N; j++)
    A[i][j] = A[i][j] + u1[i] * v1[j] + u2[i] * v2[j];

for (int i = 0; i < _PB_N; i++)
  for (int j = 0; j < _PB_N; j++)
    x[i] = x[i] + beta * A[j][i] * y[j];

for (int i = 0; i < _PB_N; i++)
  x[i] = x[i] + z[i];

for (int i = 0; i < _PB_N; i++)
  for (int j = 0; j < _PB_N; j++)
    w[i] = w[i] +  alpha * A[i][j] * x[j];
#pragma endscop
}
