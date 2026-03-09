# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF', <<'EOF', <<'EOF']);
(sc-bad-nr-1) begin
sc-bad-nr-1: exit(-1)
EOF
(sc-bad-nr-1) begin
Executed an unknown system call!
Stack top + 0: 21
Stack top + 1: 0
sc-bad-nr-1: exit(-1)
EOF
(sc-bad-nr-1) begin
Executed an unknown system call!
Stack top + 0: 22
Stack top + 1: 0
sc-bad-nr-1: exit(-1)
EOF
pass;
