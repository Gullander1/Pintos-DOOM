# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(sc-bad-close) begin
OK
sc-bad-close: exit(-1)
EOF
pass;
