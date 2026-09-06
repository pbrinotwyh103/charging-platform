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
- Existing reservation timestamp comparisons are textual against SQLite CURRENT_TIMESTAMP. Later services should supply compatible SQL UTC timestamps or separately normalize comparisons before passing ISO 8601 Z values, especially same-day expiration.
- Existing database integer/error/output-pointer conventions are preserved; null record/list pointers are not supported.
- SQLite concurrent write contention may return a database error for one competing attempt; retrying the same business key is safe and cannot double-apply effects.
- Settlement deliberately releases fault/offline piles to idle when finalStatus is fault_stopped, following the Task 2 brief. Any desired persistent unavailable status must be coordinated by the later device/service layer.
