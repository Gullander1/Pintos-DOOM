# -*- perl -*-
use strict;
use warnings;
use tests::tests;

sub check_for_fail {
    our ($test);
    my (@output) = read_text_file ("$test.output");
    common_checks ("run", @output);
    pass;
}
check_for_fail ();
