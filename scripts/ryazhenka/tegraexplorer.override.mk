#---------------------------------------------------------------------------------
# TegraExplorer build wrapper — same recipe as TegraExplorer/Makefile, with our
# Ряженка-side warning suppressions appended. TegraExplorer is a vendor submodule
# and we don't want to commit warning fixes inside it (that would diverge from
# the upstream gitlink each time we sync), so the cleanest way to silence its
# build noise is to include the original Makefile from a wrapper that appends
# the extra flags after the upstream assignments.
#---------------------------------------------------------------------------------

include $(CURDIR)/Makefile

# Suppressions for warnings emitted by the unmodified TegraExplorer tree —
# all of them are pre-existing in upstream and are not introduced by our fork.
CFLAGS  += -Wno-unused-variable \
           -Wno-int-to-pointer-cast \
           -Wno-pointer-to-int-cast \
           -Wno-duplicate-decl-specifier \
           -Wno-discarded-qualifiers

# The linker rightly complains about TegraExplorer.elf having a LOAD segment
# with RWX permissions, but that is by design for an RCM payload — it has to
# be both writable (BSS scratch) and executable (jumped into from the bootrom).
LDFLAGS += -Wl,--no-warn-rwx-segments
