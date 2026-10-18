# ShopHub - notes for Claude Code

ShopHub is a Qt 6 Widgets + SQLite e-commerce desktop app in C++17 (Windows first, Linux later).
v1 is built and tested. Work on it in small, reviewable milestones.

## Build and test

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Run the tests after every meaningful change. Never claim a build or test passed unless you ran it.
`tst_gui` runs offscreen; set `SHOPHUB_SCREENSHOT_DIR` to get PNGs of every screen.

## Architecture rules (do not break these)

1. Layers: `src/app/ui` (Qt Widgets) → `src/core/services` → `src/core/repositories` → SQLite.
2. **SQL lives only in `src/core/repositories/`** (plus migrations). Never in widgets, pages or dialogs.
3. **Business rules live only in services.** GUI slots call one service method and show the result.
4. **Every service method checks the `Session`** (`access::requireAdmin/Customer/SignedIn`).
   Hiding a button is not security.
5. `shophub_core` must never link Qt Widgets/Gui, so it stays testable without a GUI.
6. Money is `shophub::Money` (whole paisa, `qint64`). Never use `double` for money.
   The GUI never computes totals that get saved; `PricingService` / `CheckoutService` do.
7. Multi-step writes use `Transaction` (BEGIN IMMEDIATE, rolls back in its destructor).
8. Errors: services return `Result<T>` / `Status` with user-safe messages. Technical details go to
   the log (`qWarning`) only. Never log passwords, hashes or full addresses.
9. Bind optional text with `sql::text()` - Qt binds a null `QString` as SQL NULL.
10. Schema changes = a **new migration** appended in `database/Migrations.cpp`. Never edit an
    applied migration.
11. Colours come from `ui::Theme` tokens and the style sheet in `Theme.cpp`. Use `makeLabel`,
    `makeButton(text, "primary" | "danger" | "ghost" | "link" | "chip")`, `makeCard`, `makeBadge`.
    Escape `&` in button text with `escapeMnemonic()`.
12. Pages talk to each other only through `AppEvents` signals (cartChanged, ordersChanged, toast, ...).

## Conventions

- Namespaces: `shophub` (core), `shophub::ui` (GUI). Qt naming style, 4-space indent.
- New core feature → write tests in `tests/` alongside it (use `test::Backend` from `TestSupport.h`).
- Keep the README's feature list and test table up to date.

## Scope

Done in v1: auth (PBKDF2, lockout), catalog + categories + images, search/filters, persistent cart,
COD checkout (atomic, idempotent), orders + status history + cancellation with restock,
inventory movements, admin dashboard, light/dark theme, sample data (50 customers, ~70 orders).

Sample customers live in `DemoSeeder.cpp`. If you change them, update `docs/CUSTOMER_ACCOUNTS.md`
and `docs/customer_accounts.csv` too (`tst_orders_inventory` fails if they differ).

Next (v2 candidates, ask before starting): suppliers & purchase orders, coupons, wishlist, reviews,
reports + CSV/PDF export, audit log viewer, backup/restore, Argon2id, installer.
