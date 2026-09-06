# Task 2 report — transactional business repositories

Status: complete within Task 2 scope.

## Implementation

- Wallet recharge is idempotent by record number. An exact replay returns the original ledger balance, even after later wallet changes. Reusing a record number for another user, type, or amount fails without side effects.
- Order creation replays an existing order number only when user, pile, and reservation match.
- OrderRepository::stopAndSettle delegates to the existing WalletRepository transaction using ORDER-PAYMENT-{orderId}. Settlement checks order state and balance, debits funds, creates the negative ledger entry, finalizes the order, increments pile usage, and releases the pile atomically.
- Settlement replays by order ID as well as ledger key, preserving compatibility with callers of the existing WalletRepository::settleOrder API. Replays return the first payment balance and leave final quantities/reason untouched.
- Fault-stopped settlement also permits releasing fault/offline piles; normal completion keeps the charging-state requirement.
- Added wallet and reservation lookup methods, unbounded active-order recovery, matching list/count methods, and database pagination. Order pages use created_at DESC,id DESC for deterministic ordering.
- Preserved the original unpaginated station/pile APIs and all existing record layouts.

## Produced interfaces

- WalletRepository::findByRecordNo(const QString &, WalletRecord *, QString *) const
- WalletRepository::countByUser(qint64, int *, QString *) const
- ReservationRepository::findById(qint64, ReservationRecord *, QString *) const
- OrderRepository::stopAndSettle(qint64, qint64, qint64, qint64, const QString &, const QString &, qint64 *, QString *) const
- OrderRepository::listActive(QList<OrderRecord> *, QString *) const
- OrderRepository::countByUser(qint64, int *, QString *) const
- OrderRepository::list(const QString &status, int limit, int offset, QList<OrderRecord> *, QString *) const
- OrderRepository::count(const QString &status, int *, QString *) const
- UserRepository::count(const QString &phoneKeyword, int *, QString *) const
- StationRepository::list(const QString &status, int limit, int offset, QList<StationRecord> *, QString *) const
- StationRepository::count(const QString &status, int *, QString *) const
- PileRepository::listByStation(qint64, const QString &status, int limit, int offset, QList<PileRecord> *, QString *) const
- PileRepository::countByStation(qint64, const QString &status, int *, QString *) const

Empty status means all statuses. Pagination uses the existing limit clamp of 1–200 and nonnegative offsets. Active recovery is not limited to 200 rows.

## RED evidence

1. Wrote missing-interface, recharge replay, atomic rollback, restart recovery, cancellation/expiry, and pagination tests before implementation.
2. Requested top-level command:

   qmake charging-platform.pro -spec macx-clang CONFIG+=debug && make -j4 && build/bin/database_repository_tests -v1

   failed at the unrelated user-client target with: Project ERROR: Unknown module(s) in QT: webenginewidgets.
3. Built the database test target directly from tests/:

   qmake database-repository-tests.pro -spec macx-clang CONFIG+=debug && make -f Makefile -j4

   failed compilation with missing findById, findByRecordNo, countByUser, stopAndSettle, listActive, count, and pagination overloads. This confirmed the intended missing interface failures.
4. After the first implementation passed 11 tests, added faultSettlementAndLegacyReplay from the design-required fault-stop behavior. Running the suite produced 11 passed, 1 failed: settlement returned false with 电桩状态异常. Then extended the fault-stopped release predicate.

## GREEN evidence

- Direct target: make -s -j4 && ../bin/database_repository_tests -v1 from tests/.
- Result after fault-stop fix: 12 passed, 0 failed, 0 skipped.
- Moved generated binary and untracked build helpers into ignored build/ paths. Final execution: build/bin/database_repository_tests -v1.
- Final result: 12 passed, 0 failed, 0 skipped, 44 ms.
- git diff --check passed.

The tests verify original-balance replay, mismatched recharge key rejection, duplicate start, all three settlement table effects, insufficient-balance rollback, rollback after an injected failure in the final pile write, fresh-manager recovery of persisted progress, no duplicate settlement usage/ledger entries, cancellation/expiry pile release, filtered counts/pages, and fault-stop compatibility with the older wallet API.

## Files

- server/repositories/userrepository.h and .cpp
- server/repositories/walletrepository.h and .cpp
- server/repositories/reservationrepository.h and .cpp
- server/repositories/orderrepository.h and .cpp
- server/repositories/stationrepository.h and .cpp
- server/repositories/pilerepository.h and .cpp
- tests/database_repository_test.h and .cpp
- This report.

## Self-review

- All new mutations use the existing single-connection transaction convention; no nested transaction was introduced.
- The repository baseline has no DatabaseManager::transaction method, so existing QSqlDatabase::transaction/rollback is used.
- Existing repositories use bound positional parameters; extensions preserve that style.
- The strongest rollback test fails the final write after debit, ledger insert, and order finalization, then verifies all affected stored values remain unchanged.
- Global business key collisions reject mismatched ownership rather than returning another user's data.
- No schema, client protocol, service, or database backup implementation was changed.

## Concerns and integration notes

- Full top-level build remains unverified because the installed Qt lacks webenginewidgets; database target compiles and passes independently.
- The original mixed-format reservation timestamp concern was addressed in review fix round 1 below.
- Existing database integer/error/output-pointer conventions are preserved; null record/list pointers are not supported.
- SQLite concurrent write contention may return a database error for one competing attempt; retrying the same business key is safe and cannot double-apply effects.
- Settlement deliberately releases fault/offline piles to idle when finalStatus is fault_stopped, following the Task 2 brief. Any desired persistent unavailable status must be coordinated by the later device/service layer.

## Review fix round 1 — timestamp compatibility

Addressed both blocking review findings without schema or service changes.

### Changes

- Reservation consumption and expiry compare julianday values, accepting legacy SQL UTC strings, UTC ISO 8601 Z strings, and explicit timezone offsets as actual instants.
- User, wallet, reservation, order, station, and pile read queries project timestamps through SQLite strftime to UTC ISO 8601 with T/Z, at the existing seconds precision. Nullable timestamps remain empty in records.
- Settlement writes stopped_at, order/user/pile updated_at, and payment ledger created_at as UTC ISO 8601 Z values.
- Order and wallet pagination sort normalized instants with the existing ID tie-breaker. This is necessary when old rows and new settlement rows coexist.

### RED evidence

- Added mixedFormatReservationExpiry before changing production: an ISO 09:00Z expiry compared with a same-day SQL 10:00 cutoff incorrectly returned expired=0 instead of 1.
- Ran isoExpiredReservationCannotStart independently: the repository incorrectly allowed starting an ISO reservation that had expired one second earlier.
- Ran repositoryUtcTimestamps independently: a legacy created_at read returned 2026-09-06 04:05:06 instead of 2026-09-06T04:05:06Z.
- Initial UTC write changes exposed a pre-existing ordering assumption in atomicSettlementAndRecovery: the later recharge sorted below an earlier payment because of their different stored formats.
- Added mixedFormatPagination before changing order sort queries: the 09:00 ISO order was incorrectly placed before the 11:00 legacy order (actual ID 2, expected ID 1).

### GREEN evidence

Built in an ignored shadow directory:

    mkdir -p build/task-2-review
    cd build/task-2-review
    qmake ../../tests/database-repository-tests.pro -spec macx-clang CONFIG+=debug
    make -s -j4
    ../bin/database_repository_tests -v1

Result: 16 passed, 0 failed, 0 skipped, 30 ms. git diff --check passed.

New tests cover both same-day format directions, expiry equality, timezone offsets, expired ISO start rejection, future legacy start acceptance, canonical output from all six repositories, nullable timestamps, raw persisted settlement timestamps across four tables, and mixed-format pagination.

### Self-review and remaining concerns

- Comparison uses julianday rather than formatting, so timezone offsets and fractional instants retain comparison semantics.
- All timestamp fields in the six Task 2 record types are projected consistently; no record layout or interface signature changed.
- SQLite strftime defaults to UTC and preserves null as null; Qt converts nullable result strings to the existing empty-string representation.
- Exposed timestamps use seconds precision, consistent with existing SQLite timestamp generation and charging duration fields.
- Normalized ORDER BY expressions may need an expression index for much larger datasets; adding a schema/index migration is outside this review fix.
- Full top-level Qt WebEngine build limitation remains unchanged. No other review blockers remain identified in this scope.
