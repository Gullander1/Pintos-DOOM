# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF', <<'EOF']);
(sc-bad-nr-2) begin
sc-bad-nr-2: exit(-1)
EOF
(sc-bad-nr-2) begin
Executed an unknown system call!
Stack top + 0: 1048576
Stack top + 1: 0
sc-bad-nr-2: exit(-1)
EOF
pass;
