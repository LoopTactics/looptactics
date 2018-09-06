void foo(int i, int j, int k, int l);

void nested(int n) {

#pragma scop
  for (int i = 0; i < 1024; ++i) {
    for (int j = 0; j < n; ++j) {
      for (int k = n - 1; k < n + 42; k++) {
        for (int l = i + 1; l < n - j; l++) {
          foo(i, j, k, l);
        }
      }
    }
  }
#pragma endscop
}
