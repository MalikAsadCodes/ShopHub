# Qist - installments (v2 milestone 3)

Across Pakistan, phones, fridges, motorbikes and furniture are sold on **qist**: the
customer pays part of the price now and the rest in fixed monthly installments, and the
shop keeps a card for every buyer. This milestone puts that card inside ShopHub.
**Status: done.**

## Decisions

| # | Question | Decision |
|---|---|---|
| 1 | Down payment? | **Yes.** Each plan has a minimum down payment as a percentage (for example 20%). The customer may pay more. |
| 2 | Markup? | **A fixed percentage per plan** (it can be 0%). The qist price is worked out once, when the order is placed, and never grows afterwards. |
| 3 | Plans | **The admin creates them**, for example 3, 6 or 12 months. |
| 4 | Late payment | **No fine.** An unpaid installment past its due date shows as **Overdue**, and the customer cannot start a new qist until it is paid. |
| 5 | Khata link | Every qist goes into the customer's **khata ledger**: the qist price when the order is placed, then every installment received. |
| 6 | Which products? | Only products the admin marks **"Can be sold on qist"**, usually phones and appliances, not groceries. |
| 7 | Which customers? | Only customers the admin **allows qist** for, the same way a khata is opened. |

## How it works

Example: Bilal buys a phone for **Rs 60,000** on the plan *"6 months: 10% markup, at least
20% down"*.

| Step | Amount |
|---|---|
| Cash price (the order total) | Rs 60,000 |
| Markup, 10% of the cash price | Rs 6,000 |
| **Qist price** | **Rs 66,000** |
| Down payment, at least 20% of the qist price | Rs 13,200, due on the order date |
| Left to pay | Rs 52,800 |
| Monthly installment, 6 months | **Rs 8,800**, due on the same date each month |

When the remaining amount does not divide evenly, the regular installments are rounded
**down to whole rupees** and the last one takes the rest, so the schedule always adds up to
the qist price exactly (for example 5 × Rs 3,226 and a last installment of Rs 3,228.90).

### Rules

- Money stays exact (whole paisa). The markup is rounded half-up, the same way tax is; the
  minimum down payment is rounded **up** to the next whole rupee.
- A plan runs for **2 to 24 months**, with a markup of **0% to 50%** and a minimum down
  payment of **0% to 90%**.
- Each monthly installment must be at least **Rs 100**; otherwise the customer is asked to
  choose a shorter plan or a smaller down payment.
- The down payment must be less than the qist price (to pay everything now, choose Cash on
  Delivery).
- Every product in the cart must allow qist, the customer must be allowed qist, and they must
  have **no overdue installment**. These checks run inside the checkout transaction.
- The order keeps the **cash price** as its total; the qist (plan, markup, schedule) is
  stored next to it and **never changes** (the database refuses edits to the terms and the
  schedule).
- Payments are applied to the **oldest installment first**. A payment can be any amount up
  to what is still owed.
- The admin records the **down payment before marking the order Delivered**, the same way a
  shop hands over the goods once the down payment is in hand.
- When the last rupee is paid the qist is **Completed** and the order is marked **Paid**.
- **Cancelling** a qist order (before it ships) takes the qist off the khata, and any money
  already paid is shown as returned to the customer.

### Khata and qist together

The khata ledger now holds two kinds of entries: normal khata entries and qist entries
(marked with the qist they belong to).

- **What the customer owes** = everything on the ledger, khata and qist together.
- **Available credit** = credit limit minus the **khata part only**. Buying on qist never
  uses up the khata credit limit.
- **Record payment** on the Khata page pays the khata part. Qist installments are recorded
  on the **Qist** page, so every payment lands on the right installment.

## Screens

| Who | Where | What |
|---|---|---|
| Admin | **Qist** page | Every qist with paid, left, next due date and status (Active, Overdue, Completed, Cancelled); the schedule of the selected one; **Record installment...**; **Plans...** to add, edit or switch off plans |
| Admin | **Khata** page | **Allow qist** / **Stop qist** for the selected customer; a *Qist* column |
| Admin | **Edit product** | "Can be sold on qist" tick; a *Qist* column on the Products page |
| Customer | Shop | **Qist** badge on products that allow it; a plan table on the product page |
| Customer | Checkout | **Qist (installments)**: choose a plan and the down payment, see the schedule before placing the order |
| Customer | **My qist** page | Each qist with its schedule: paid, due, overdue |

## Data

| Table / column | What it holds |
|---|---|
| `qist_plans` | Name, months, markup and minimum down payment (basis points), on/off |
| `qist_accounts` | One per qist order: the terms copied from the plan, cash price, markup, qist price, down payment and status (`active`, `completed`, `cancelled`) |
| `qist_installments` | The schedule: number (0 = down payment), due date, amount. Never changes. |
| `khata_entries.qist_id` | Links a ledger entry to its qist |
| `products.qist_allowed` | The product can be sold on qist |
| `khata_accounts.qist_approved` | The customer is allowed qist |
| `orders.payment_method` | Now also `qist` |

Paid amounts are not stored twice: they are the sum of the qist's payment entries in the
khata ledger.

## Plan

| Step | Status |
|---|---|
| Rules written down (this file) | **Done** |
| Qist maths: terms, quote, schedule, applying payments, overdue (`src/core/domain/Qist.h`) | **Done** |
| Database: migration 5 (`qist_plans`, `qist_accounts`, `qist_installments`, product and customer flags, `qist` payment method, ledger link) | **Done** |
| Repository and service: plans, approval, quotes, accounts, payments (`QistRepository`, `QistService`) | **Done** |
| Checkout with installments; cancelling a qist order; down payment before delivery | **Done** |
| Khata: available credit uses the khata part only | **Done** |
| Admin UI: Qist page, plans, record installment, Allow qist, product tick | **Done** |
| Customer UI: badge, plan table, checkout plan choice, My qist page | **Done** |
| Sample data: 3 plans, 10 qist products, 6 approved customers, 5 qists (completed, on time, overdue, new) | **Done** |
| Sample data reaches databases that already exist (sample data version 3) | **Done** |
| Tests: maths, database rules, service rules, checkout, cancel, sample data, GUI journey | **Done** |
| README, `DATABASE.md`, `ARCHITECTURE.md`, `CUSTOMER_ACCOUNTS.md` updated | **Done** |

## Later (not in this milestone)

- **Guarantor and CNIC copy** on a qist, as many shops ask for.
- **Reminders** before a due date (v4: WhatsApp share and khata reminders).
- **Early settlement discount** when a customer pays off a qist early.
