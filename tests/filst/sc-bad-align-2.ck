# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(sc-bad-align-2) begin
sc-bad-align-2: exit(-1)
EOF
pass;
