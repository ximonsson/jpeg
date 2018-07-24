function y = fdct3 (x);
    y = zeros (8);
    for i = 1:8,
        y (i, :) = fdct (x (i, :));
    end
    for i = 1:8,
        y (:, i) = fdct (y (:, i));
    end
