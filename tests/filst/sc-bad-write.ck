# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(sc-bad-write) begin
WORKS
sc-bad-write: exit(-1)
EOF
pass;
