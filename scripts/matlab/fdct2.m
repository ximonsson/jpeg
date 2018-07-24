function y = fdct2 (x) ;
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
    % transform
    y = C * x * C';

    % lal = zeros (64, 1);
    % for i = 1:8,
    %     lal (1 + (i - 1) * 8: i * 8) = x (:, i);
    % end
    % y = kron (C, C) * lal;
    % lol = zeros (8, 8);
    % for i = 1:8,
    %     lol (:, i) = y (1 + (i - 1) * 8 : i * 8);
    % end
    % y = lol;
    % C
