# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(sc-bad-exit) begin
sc-bad-exit: exit(-1)
EOF
pass;
