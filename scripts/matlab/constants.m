c1 = ga (1);
c2 = ga (2);
c3 = ga (3);
c4 = ga (4);
c5 = ga (5);
c6 = ga (6);
c7 = ga (7);

% C8
C = [[ c4,  c4,  c4,  c4,  c4,  c4,  c4,  c4];
     [ c1,  c3,  c5,  c7, -c7, -c5, -c3, -c1];
     [ c2,  c6, -c6, -c2, -c2, -c6,  c6,  c2];
     [ c3, -c7, -c1, -c5,  c5,  c1,  c7, -c3];
     [ c4, -c4, -c4,  c4,  c4, -c4, -c4,  c4];
     [ c5, -c1,  c7,  c3, -c3, -c7,  c1, -c5];
     [ c6, -c2,  c2, -c6, -c6,  c2, -c2,  c6];
     [ c7, -c5,  c3, -c1,  c1, -c3,  c5, -c7]];

P  = [[1, 0, 0, 0,  0,  0, 0,  0];
      [0, 0, 0, 0, -1,  0, 0,  0];
      [0, 0, 1, 0,  0,  0, 0,  0];
      [0, 0, 0, 0,  0, -1, 0,  0];
      [0, 1, 0, 0,  0,  0, 0,  0];
      [0, 0, 0, 0,  0,  0, 0, -1];
      [0, 0, 0, 1,  0,  0, 0,  0];
      [0, 0, 0, 0,  0,  0, 1,  0]];

B3 = [[1, 0, 0, 0,  0,  0,  0,  1];
      [0, 1, 0, 0,  0,  0,  1,  0];
      [0, 0, 1, 0,  0,  1,  0,  0];
      [0, 0, 0, 1,  1,  0,  0,  0];
      [1, 0, 0, 0,  0,  0,  0, -1];
      [0, 1, 0, 0,  0,  0, -1,  0];
      [0, 0, 1, 0,  0, -1,  0,  0];
      [0, 0, 0, 1, -1,  0,  0,  0]];
% B2
B2 = [[1, 0,  0,  1, 0, 0, 0, 0];
      [0, 1,  1,  0, 0, 0, 0, 0];
      [1, 0,  0, -1, 0, 0, 0, 0];
      [0, 1, -1,  0, 0, 0, 0, 0];
      [0, 0,  0,  0, 1, 0, 0, 0];
      [0, 0,  0,  0, 0, 1, 0, 0];
      [0, 0,  0,  0, 0, 0, 1, 0];
      [0, 0,  0,  0, 0, 0, 0, 1]];

B1 = [[ 1, 1, 0,  0,  0,  0,  0, 0];
      [-1, 1, 0,  0,  0,  0,  0, 0];
      [ 0, 0, 0,  1,  0,  0,  0, 0];
      [ 0, 0, 1,  0,  0,  0,  0, 0];
      [ 0, 0, 0,  0,  0,  0, -1, 0];
      [ 0, 0, 0,  0,  0,  0,  0, 1];
      [ 0, 0, 0,  0,  0, -1,  0, 0];
      [ 0, 0, 0,  0, -1,  0,  0, 0]];

G2 = [[c6, c2]; [-c2, c6]];
G4 = [[ c5, -c7,  c3,  c1];
      [-c1,  c5, -c7,  c3];
      [-c3, -c1,  c5, -c7];
      [ c7, -c3, -c1,  c5]];

K = zeros (8);
K (1, 1) = c4;
K (2, 2) = c4;
K (3:4, 3:4) = G2;
K (5:8, 5:8) = G4;
K = .5 * K;

M = zeros(8);
M (1:4, 1:4) = [[c4  c4  c4  c4];
                [c2  c6 -c6 -c2];
                [c4 -c4 -c4  c4];
                [c6 -c2  c2 -c6]];
M (5:8, 5:8) = [[c1  c3  c5  c7];
                [c3 -c7 -c1 -c5];
                [c5 -c1  c7  c3];
                [c7 -c5  c3 -c1]];

Q = [[8.0,  5.0,  5.0,  8.0, 12.0, 20.0, 25.0, 30.0];
     [6.0,  6.0,  7.0,  9.0, 13.0, 29.0, 30.0, 27.0];
     [7.0,  6.0,  8.0, 12.0, 20.0, 28.0, 34.0, 28.0];
     [7.0,  8.0, 11.0, 14.0, 25.0, 43.0, 40.0, 31.0];
     [9.0, 11.0, 18.0, 28.0, 34.0, 54.0, 51.0, 38.0];
    [12.0, 17.0, 27.0, 32.0, 40.0, 52.0, 56.0, 46.0];
    [24.0, 32.0, 39.0, 43.0, 51.0, 60.0, 60.0, 50.0];
    [36.0, 46.0, 47.0, 45.0, 56.0, 50.0, 51.0, 49.0]];