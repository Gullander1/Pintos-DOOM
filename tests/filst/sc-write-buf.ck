# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(sc-write-buf) begin
sc-write-buf: exit(-1)
EOF
pass;
