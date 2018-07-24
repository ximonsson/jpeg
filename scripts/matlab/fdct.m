% From:
% https://books.google.se/books?id=1fDLBQAAQBAJ&pg=PT71&lpg=PT71&dq=feig+winograd+dct&source=bl&ots=McXjg-P3Ob&sig=ZF_zCOyC6BCVLWXrgI_-sOGdnfw&hl=en&sa=X&ved=0ahUKEwjhloSJhLvJAhUBGRQKHSngCEs4ChDoAQgiMAE#v=twopage&q&f=false
function y = fdct (x);

% constants
C1 = 1 / cos (    pi / 16);
C2 = 1 / cos (    pi /  8);
C3 = 1 / cos (3 * pi / 16);
C4 =     cos (    pi /  4);
C5 = 1 / cos (5 * pi / 16);
C6 = 1 / cos (3 * pi /  8);
C7 = 1 / cos (7 * pi / 16);

% multiply by B3
A1 = x(1) + x(8);
A2 = x(2) + x(7);
A3 = x(3) + x(6);
A4 = x(4) + x(5);
A5 = x(1) - x(8);
A6 = x(2) - x(7);
A7 = x(3) - x(6);
A8 = x(4) - x(5);

% multiply by B2
A9  = A1 + A4;
A10 = A2 + A3;
A11 = A1 - A4;
A12 = A2 - A3;

% multiply by B1
A13 = A9 + A10;
A14 = A9 - A10;

% multiply by 1/2 G1
M1 = (1 / 2) * C4 * A13; % y(1)
M2 = (1 / 2) * C4 * A14; % y(5)

% multiply by 1/2 G2
A15 = -A12 + A11;
M3 = C4 * A15;
A20 = A12 + M3;
A21 = -A12 + M3;
M6 = (1 / 4) * C6 * A20; % y(3)
M7 = (1 / 4) * C2 * A21; % y(7)

% multiply by 1/2 G4
% H_42
A16 = A8 - A5;
A17 = -A7 + A5;
A18 = A8 + A6;
A19 = -A17 + A18;

% multiply by 1, G1, G2
M4 = C4 * A16;
M5 = C4 * A19;
A22 = A17 + M5;
A23 = -A17 + M5;
M8 = (1 / 2) * C6 * A22;
M9 = (1 / 2) * C2 * A23;

% multiply by H_41, then by D^-1, and then 1/2 this is G4
% then multiply by 1/2 to make it 1/2 G4
A24 = -A7  + M4;
A25 =  A7  + M4;
A26 =  A24 - M8;
A27 =  A25 + M9;
A28 = -A24 - M8;
A29 = -A25 + M9;

M10 = -(1 / 4) * C5 * A26; % y(2)
M11 = -(1 / 4) * C1 * A27; % y(4)
M12 =  (1 / 4) * C3 * A28; % y(6)
M13 = -(1 / 4) * C7 * A29; % y(8)

y(1) = M1;
y(2) = M10;
y(3) = M6;
y(4) = M11;
y(5) = M2;
y(6) = M13;
y(7) = M7;
y(8) = M12;
