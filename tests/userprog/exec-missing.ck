# -*- perl -*-

# In this test a program named "exec-missing" tries to start another
# program that do not exist. This is discovered in the thread of the
# new program, in the function load.

# Correct handling can be done in three basic ways:

# 1. We only know about the failure when our "exec" system call
#    returns -1. The "no-such-file" thread never becomes a process and
#    do not leave any messages.

# 2. We know about the failure both when "exec" return -1 and when the
#    "no-such-file" thread fails to become a process. It notify the
#    failure by printing "load: no-such-file: open failed". But it
#    does not become a process and will thus not leave a
#    "no-such-file: exit(-1)" message.

# 3. "exec" return -1 and "no-such-file" become a failed process. It
#    will thus produce a "no-such-file: exit(-1)" message. This
#    message may occur some time after "open failed".

use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF', <<'EOF', <<'EOF', <<'EOF', <<'EOF', <<'EOF']);
(exec-missing) begin
(exec-missing) exec("no-such-file"): -1
(exec-missing) end
exec-missing: exit(0)
EOF
(exec-missing) begin
load: no-such-file: open failed
(exec-missing) exec("no-such-file"): -1
(exec-missing) end
exec-missing: exit(0)
EOF
(exec-missing) begin
load: no-such-file: open failed
no-such-file: exit(-1)
(exec-missing) exec("no-such-file"): -1
(exec-missing) end
exec-missing: exit(0)
EOF
(exec-missing) begin
load: no-such-file: open failed
(exec-missing) exec("no-such-file"): -1
no-such-file: exit(-1)
(exec-missing) end
exec-missing: exit(0)
EOF
(exec-missing) begin
load: no-such-file: open failed
(exec-missing) exec("no-such-file"): -1
(exec-missing) end
no-such-file: exit(-1)
exec-missing: exit(0)
EOF
(exec-missing) begin
load: no-such-file: open failed
(exec-missing) exec("no-such-file"): -1
(exec-missing) end
exec-missing: exit(0)
no-such-file: exit(-1)
EOF
pass;
