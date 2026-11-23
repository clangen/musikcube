#include <common/DeleteDefaults.h>
#include <type_traits>

/* deleting copy operations must still allow explicitly defaulted moves. */
struct MoveOnly {
    MoveOnly() = default;
    MUSIK_DELETE_COPY(MoveOnly);
    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly&&) = default;
};
static_assert(!std::is_copy_constructible<MoveOnly>::value, "copy construction is disabled");
static_assert(!std::is_copy_assignable<MoveOnly>::value, "copy assignment is disabled");
static_assert(std::is_move_constructible<MoveOnly>::value, "explicit move construction remains available");
static_assert(std::is_move_assignable<MoveOnly>::value, "explicit move assignment remains available");

/* an explicit move prohibition must reject rvalues even when copying works. */
struct CopyOnly {
    CopyOnly() = default;
    CopyOnly(const CopyOnly&) = default;
    CopyOnly& operator=(const CopyOnly&) = default;
    MUSIK_DELETE_MOVE(CopyOnly);
};
static_assert(std::is_copy_constructible<CopyOnly>::value, "explicit copy construction remains available");
static_assert(std::is_copy_assignable<CopyOnly>::value, "explicit copy assignment remains available");
static_assert(!std::is_move_constructible<CopyOnly>::value, "move construction cannot fall back to copying");
static_assert(!std::is_move_assignable<CopyOnly>::value, "move assignment cannot fall back to copying");

/* fixed owners cannot be copied or moved; the macro adds no default constructor. */
struct FixedOwner {
    explicit FixedOwner(int) {}
    MUSIK_DELETE_COPY_AND_MOVE(FixedOwner);
};
static_assert(!std::is_default_constructible<FixedOwner>::value, "constructor policy stays explicit");
static_assert(std::is_constructible<FixedOwner, int>::value, "declared constructor remains usable");
static_assert(!std::is_copy_constructible<FixedOwner>::value && !std::is_copy_assignable<FixedOwner>::value,
    "fixed ownership rejects copying");
static_assert(!std::is_move_constructible<FixedOwner>::value && !std::is_move_assignable<FixedOwner>::value,
    "fixed ownership rejects moving");

/* existing macro entry points keep their constructor policies and prohibit both
copy and move operations while delegating to the same declaration helpers. */
struct DefaultOwner {
    DELETE_COPY_AND_ASSIGNMENT_DEFAULTS_WITH_DEFAULT_CTOR(DefaultOwner)
};
struct StaticOnly {
    DELETE_CLASS_DEFAULTS(StaticOnly)
};
static_assert(std::is_nothrow_default_constructible<DefaultOwner>::value, "default-ctor policy is preserved");
static_assert(!std::is_copy_constructible<DefaultOwner>::value && !std::is_move_constructible<DefaultOwner>::value,
    "default-ctor helper still fixes ownership");
static_assert(!std::is_default_constructible<StaticOnly>::value, "static-only helper prohibits construction");

/* successful compilation verifies the special-member contracts above. */
int main() {
    return 0;
}
