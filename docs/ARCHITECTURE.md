# ShopHub architecture

## Layers

```
┌───────────────────────────────────────────────────────────────┐
│  GUI  (src/app/ui, library shophub_ui, Qt Widgets)             │
│  MainWindow · pages · dialogs · widgets · table models        │
│  Shows data, collects input, calls ONE service method.        │
└───────────────┬───────────────────────────────────────────────┘
                │  AppContext& + Session
┌───────────────▼───────────────────────────────────────────────┐
│  Services  (src/core/services)                                │
│  Auth · Catalog · Cart · Pricing · Checkout · Orders ·        │
│  Inventory · Khata · Dashboard · ImageStore · DemoSeeder      │
│  Business rules, validation, role checks, transactions.       │
└───────────────┬───────────────────────────────────────────────┘
                │
┌───────────────▼───────────────────────────────────────────────┐
│  Repositories  (src/core/repositories)  - the only SQL        │
│  Users · Categories · Products · Carts · Orders ·             │
│  Addresses · Audit · Settings · Khata                         │
└───────────────┬───────────────────────────────────────────────┘
                │
┌───────────────▼───────────────────────────────────────────────┐
│  Database  (src/core/database)                                │
│  Database (connection, FK on) · Transaction · Migrator        │
└───────────────┬───────────────────────────────────────────────┘
                ▼
            SQLite file  (%APPDATA%\ShopHub\ShopHub\shophub.db)
```

`shophub_core` (everything below the GUI) links only Qt Core, Sql and Network, so every rule
can be tested without opening a window. `AppContext` builds and owns the whole back end;
`main.cpp` creates it once and hands a reference to the GUI.

## Responsibilities

| Component | Responsibility |
|---|---|
| `Money` | Exact paisa arithmetic, parsing user input, `Rs 1,234.50` formatting, half-up percentages |
| `Result<T>` / `Status` | Return values carrying a user-safe error message |
| `Session` | Who is signed in and their role; passed to every service call |
| `Database` | Opens SQLite, enables and **verifies** foreign keys, WAL mode, busy timeout |
| `Transaction` | `BEGIN IMMEDIATE`; rolls back automatically if `commit()` is not reached |
| `Migrator` | Applies numbered migrations once each, recorded in `schema_migrations` |
| `PasswordHasher` | PBKDF2-HMAC-SHA512, 210k iterations, 16-byte salt, constant-time compare, rehash on login if parameters were raised |
| `AuthService` | Register, login, lockout (5 failures → 15 min), first-admin setup, password change, audit |
| `CatalogService` | Search/filter/sort/page, product + category admin, wholesale price tiers, hides cost price and inactive items from customers |
| `PricingService` | Wholesale tier prices per cart line, subtotal, delivery (flat / free above threshold), tax on goods |
| `CartService` | Persistent cart with stock and per-line limits |
| `CheckoutService` | Validates and places an order atomically and idempotently |
| `OrderService` | Order lists/details with ownership checks, status transitions, cancellation with restock |
| `KhataService` | Credit limits, payments and adjustments for the khata (credit book); customers see only their own |
| `InventoryService` | Stock adjustments with reasons; every change is a movement row |
| `DashboardService` | Figures computed from stored orders |
| `ImageStore` | Validates image files by content and copies them into the app's data folder |

## Key decisions

- **Money in paisa (`qint64`).** No floating point anywhere money is calculated or stored.
  Database CHECK constraints also verify `total = subtotal + shipping + tax` and
  `line_total = unit_price × quantity`.
- **Checkout is a single transaction.** Order header, lines, stock deduction (an
  `UPDATE ... WHERE stock >= qty`, so stock can never go negative), stock movements, history,
  cart clearing and the saved address commit together or not at all. Tests inject failures at
  three points to prove it.
- **Idempotent checkout.** The checkout dialog creates a token; the `orders.checkout_token`
  column is UNIQUE and checked first, so a double click returns the existing order.
- **Snapshots.** Order lines copy SKU, name and price; a trigger blocks updates to them. Delivery
  address is copied onto the order.
- **Khata is an append-only ledger.** Each customer's balance is the sum of their
  `khata_entries`; nothing is stored that could drift from it. Entries are never updated or
  deleted (database triggers enforce it): a payment, a cancelled order or a correction is a new
  entry. The credit limit is checked inside the checkout transaction, so the limit cannot be
  beaten by two orders at once, and the order and its khata charge commit together. Cancelling a
  khata order adds a reversal in the same transaction as the restock. If an order is cancelled
  after the customer already paid, the balance goes below zero: the shop holds an advance that the
  next khata order uses first. A delivered khata order stays "on khata" (only COD becomes "paid").
- **Role checks in services.** The GUI hides admin pages from customers, but the services refuse
  admin operations from a customer session regardless.
- **PBKDF2 instead of Argon2id for v1.** PBKDF2 is built into Qt (`QPasswordDigestor`), so there
  is no extra dependency. The stored hash includes the algorithm and iteration count, so moving
  to Argon2id later is a migration-free upgrade on next login.
- **No sample admin account.** First run always asks the owner to create the admin. Sample data
  (optional) adds 50 customers with Pakistani names on `@example.com` addresses, a domain reserved
  for examples. Their passwords follow a public rule (first name + `123`), so
  `AuthService::createSampleCustomer` is admin-only, skips the strength rule only for sample data and
  hashes with fewer iterations so setup takes seconds; each hash is upgraded to full strength on that
  customer's first sign-in. `docs/customer_accounts.csv` is checked against the app by a test.
  Because khata entries can never be edited, sample data cannot be backdated afterwards like orders
  are; instead `DemoSeeder` uses `sql::ScopedFixedClock` so every sample record is written with its
  intended date. Nothing else uses that clock.
- **Local only.** SQLite is a single-computer database. Several computers would need a server
  with an authenticated API; the service layer is the natural boundary for that split.

## Known risks / limitations

| Risk | Mitigation / plan |
|---|---|
| Shared database on a network drive could corrupt | Documented as unsupported; client-server in a later version |
| Long operations on the GUI thread | v1 operations are small local queries; move to worker threads when reports/exports arrive |
| PBKDF2 weaker than Argon2id against GPU attacks | 210k SHA-512 iterations per OWASP; upgrade path built in |
| Windows build only verified by the developer, not CI | Add a GitHub Actions Windows job |
