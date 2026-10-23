# Wholesale prices (v2 milestone 2)

Pakistani shops have always given a better rate to customers who buy by the dozen or the
carton. This milestone lets the shop set **quantity prices** on any product, and checkout
applies them automatically.

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
- **The order keeps the price that was paid.** Changing or removing a tier later never
  changes an old order, just like other order lines today.
- The final price is worked out by the services inside the checkout transaction, never by
  the screen.

## Plan

| Step | Status |
|---|---|
| Tier type and price calculation (`src/core/domain/PriceTier.h`) | **Done** |
| Database: migration 3 adds `product_price_tiers` (product, starting quantity, discount) | To do |
| Repository and catalog service: load and save a product's tiers (admin only) | To do |
| Pricing, cart and checkout use the tier price; the order line stores it | To do |
| Admin: tier table on the product edit screen, with the cost-price warning | To do |
| Customer: tier table on the product page, "add 3 more for Rs 1,187.50 each" in the cart | To do |
| Sample data: tiers on some products, a few sample orders that use them | To do |
| Tests: rounding, tier boundaries, several lines, price snapshots, permissions, GUI | To do |
| README, `DATABASE.md` and `ARCHITECTURE.md` updated | To do |

## Later (not in this milestone)

- **Dealer prices:** customers marked as dealers always get a special price list.
- **Supplier buying tiers** (supplier module): what a supplier charges the shop, e.g. "100 or
  more at Rs 800 each". These will live in their **own table**, separate from the selling
  tiers above, and feed the product's cost price.
- **Supplier resale rules:** if a supplier limits what the shop may charge customers, that
  becomes a business rule in the supplier module.
- **Mixed-cart tiers:** "any 12 items" discounts. These would fit better with coupons and
  promotions.
