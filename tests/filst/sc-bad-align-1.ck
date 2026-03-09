# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(sc-bad-align-1) begin
sc-bad-align-1: exit(-1)
EOF
pass;
