# Wholesale prices (v2 milestone 2)

Pakistani shops have always given a better rate to customers who buy by the dozen or the
carton. This milestone lets the shop set **quantity prices** on any product, and checkout
applies them automatically. **Status: done.**

## Decisions

| Question | Decision |
|---|---|
| Who gets the lower price? | **Anyone** who buys enough of an item. No special customer group (yet). |
| How are tiers entered? | **Percent off** per product, e.g. "12 or more: 5% off". If the normal price changes later, the tier prices follow. |
| What counts toward the quantity? | **The same product** in one order line, not the whole cart. |
| Free delivery threshold | Uses the **discounted** total, because that is what the customer actually pays. |
| A tier below the cost price | Allowed, but the admin sees a **warning**. Sometimes selling at a loss is a choice. |

## How it works

A product can have up to 5 tiers. The tier with the highest starting quantity that the
customer reaches is the one that applies.

Example: an item normally sells for **Rs 1,250**.

| Tier | Quantity | Unit price |
|---|---|---|
| Normal price | 1 - 11 | Rs 1,250.00 |
| 12 or more: 5% off | 12 - 49 | Rs 1,187.50 |
| 50 or more: 10% off | 50 - 99 | Rs 1,125.00 |

Buying 12 costs 12 × Rs 1,187.50 = **Rs 14,250.00** instead of Rs 15,000.00.

### Rules

- Money stays exact: the discount is calculated in whole **paisa** and rounded half-up, the
  same way tax is. A price can never drop below 1 paisa.
- A tier starts at **2 to 99 units** (one cart line holds at most 99).
- A discount is **more than 0% and at most 90%**.
- No two tiers start at the same quantity, and buying more must never cost more per unit.
- These rules are checked twice: by `pricetier::validate` (which gives the admin a clear
  message) and by the database itself (CHECK constraints and triggers, migrations 3 and 4).
  A test runs the same lists through both so the two copies can never drift apart.
- **The order keeps the price that was paid.** Changing or removing a tier later never
  changes an old order, just like other order lines today.
- The final price is worked out by the services inside the checkout transaction, never by
  the screen.

## Plan

| Step | Status |
|---|---|
| Tier type and price calculation (`src/core/domain/PriceTier.h`) | **Done** |
| Database: migration 3 adds `product_price_tiers` (product, starting quantity, discount) | **Done** |
| Repository: load and save a product's tiers (`ProductRepository::tiersFor`, `replaceTiers`) | **Done** |
| Database: migration 4 adds triggers for "at most 5 tiers" and "more units never cost more" | **Done** |
| Catalog service: `priceTiers`, `setPriceTiers` (admin only, validated, audited) and `belowCostWarnings` | **Done** |
| Pricing, cart and checkout use the tier price; the order line stores it (`CartService::pricedCart`, `PricingService::applyTiers`) | **Done** |
| Admin: **Quantity prices...** editor on the Products page, live price each, cost-price warning, "Qty prices" column | **Done** |
| Customer: "Wholesale 12+" badge, *Buy more, pay less* table on the product page, "Buy 3 more to pay Rs 1,187.50 each" with an **Add** button in the cart, savings in cart and checkout | **Done** |
| Sample data: tiers on 6 products, 3 bulk orders that use them (khata totals unchanged) | **Done** |
| Tests: rounding, tier boundaries, several lines, price snapshots, permissions | **Done** |
| Tests: GUI journey from the admin editor to checkout | **Done** |
| README, `DATABASE.md` and `ARCHITECTURE.md` updated | **Done** |

## Later (not in this milestone)

- **Dealer prices:** customers marked as dealers always get a special price list.
- **Supplier buying tiers** (supplier module): what a supplier charges the shop, e.g. "100 or
  more at Rs 800 each". These will live in their **own table**, separate from the selling
  tiers above, and feed the product's cost price.
- **Supplier resale rules:** if a supplier limits what the shop may charge customers, that
  becomes a business rule in the supplier module.
- **Mixed-cart tiers:** "any 12 items" discounts. These would fit better with coupons and
  promotions.
