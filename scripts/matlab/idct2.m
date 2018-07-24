function y = idct2 (x) ;
    % constants
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
    C = C * .5;
    % C8 transposed
    Ct = transpose (C);
    % transform
    % y = Ct * x * C;    

    y = zeros (8);
    for i = 1:8,
        y (i, :) = idct_1d (x (i, :));
    end
    for i = 1:8,
        y (:, i) = idct_1d (y (:, i));
    end
